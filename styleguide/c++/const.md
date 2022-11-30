# Using Const Correctly

The **TLDR**, as stated in [C++ Dos and Don'ts](c++-dos-and-donts.md):
*** promo
For safety and simplicity, **don't return pointers or references to non-const
objects from const methods**. Within that constraint, **mark methods as const
where possible**.  **Avoid `const_cast` to remove const**, except when
implementing non-const getters in terms of const getters.
***

## A brief primer on const

To the compiler, the `const` qualifier on a method refers to _physical
constness_: calling this method does not change the bits in this object.  What
we want is _logical constness_, which is only partly overlapping: calling this
method does not affect the object in ways callers will notice, nor does it give
you a handle with the ability to do so.

Mismatches between these concepts can occur in both directions.  When something
is logically but not physically const, C++ provides the `mutable` keyword to
silence compiler complaints.  This is valuable for e.g. cached calculations,
where the cache is an implementation detail callers do not care about.  When
something is physically but not logically const, however, the compiler will
happily accept it, and there are no tools that will automatically save you.
This discrepancy usually involves pointers.  For example,

```cpp
void T::Cleanup() const { delete pointer_member_; }
```

Deleting a member is a change callers are likely to care about, so this is
probably not logically const.  But because `delete` does not affect the pointer
itself, but only the memory it points to, this code is physically const, so it
will compile.

Or, more subtly, consider this pseudocode from a node in a tree:

```cpp
class Node {
 public:
  void RemoveSelf() { parent_->RemoveChild(this); }
  void RemoveChild(Node* node) {
    if (node == left_child_)
      left_child_ = nullptr;
    else if (node == right_child_)
      right_child_ = nullptr;
  }
  Node* left_child() const { return left_child_; }
  Node* right_child() const { return right_child_; }

 private:
  Node* parent_;
  Node* left_child_;
  Node* right_child_;
};
```

The `left_child()` and `right_child()` methods don't change anything about
`|this|`, so making them `const` seems fine.  But they allow code like this:

```cpp
void SignatureAppearsHarmlessToCallers(const Node& node) {
  node.left_child()->RemoveSelf();
  // Now |node| has no |left_child_|, despite having been passed in by const ref.
}
```

The original class definition compiles, and looks locally fine, but it's a
timebomb: a const method returning a handle that can be used to change the
system in ways that affect the original object.  Eventually, someone will
actually modify the object, potentially far away from where the handle is
obtained.

These modifications can be difficult to spot in practice.  As we see in the
previous example, splitting related concepts or state (like "a tree") across
several objects means a change to one object affects the behavior of others.
And if this tree is in turn referred to by yet more objects (e.g. the DOM of a
web page, which influences all sorts of other data structures relating to the
page), then small changes can have visible ripples across the entire system.  In
a codebase as complex as Chromium, it can be almost impossible to reason about
what sorts of local changes could ultimately impact the behavior of distant
objects, and vice versa.

"Logically const correct" code assures readers that const methods will not
change the system, directly or indirectly, nor allow callers to easily do so.
They make it easier to reason about large-scale behavior.  But since the
compiler verifies physical constness, it will not guarantee that code is
actually logically const.  Hence the recommendations here.

## Classes of const (in)correctness

In a
[larger discussion of this issue](https://groups.google.com/a/chromium.org/d/topic/platform-architecture-dev/C2Szi07dyQo/discussion),
Matt Giuca
[postulated three classes of const(in)correctness](https://groups.google.com/a/chromium.org/d/msg/platform-architecture-dev/C2Szi07dyQo/lbHMUQHMAgAJ):

* **Const correct:** All code marked "const" is logically const; all code that
  is logically const is marked "const".
* **Const okay:** All code marked "const" is logically const, but not all code
  that is logically const is marked "const".  (Basically, if you see "const" you
  can trust it, but sometimes it's missing.)
* **Const broken:** Some code marked "const" is not logically const.

The Chromium codebase currently varies. A significant amount of Blink code is
"const broken". A great deal of Chromium code is "const okay". A minority of
code is "const correct".

While "const correct" is ideal, it can take a great deal of work to achieve.
Const (in)correctness is viral, so fixing one API often requires a yak shave.
(On the plus side, this same property helps prevent regressions when people
actually use const objects to access the const APIs.)

At the least, strive to convert code that is "const broken" to be "const okay".
A simple rule of thumb that will prevent most cases of "const brokenness" is for
const methods to never return pointers to non-const objects.  This is overly
conservative, but less than you might think, due to how objects can transitively
affect distant, seemingly-unrelated parts of the system.  The discussion thread
linked above has more detail, but in short, it's hard for readers and reviewers
to prove that returning pointers-to-non-const is truly safe, and will stay safe
through later refactorings and modifications.  Following this rule is easier
than debating about whether individual cases are exceptions.

One way to ensure code is "const okay" would be to never mark anything const.
This is suboptimal for the same reason we don't choose to "never write comments,
so they can never be wrong".  Marking a method "const" provides the reader
useful information about the system behavior.  Also, despite physical constness
being different than logical constness, using "const" correctly still does catch
certain classes of errors at compile time. Accordingly, the
[Google style guide requests the use of const where possible](http://google.github.io/styleguide/cppguide.html#Use_of_const),
so mark methods const when they are logically const.

Making code more const correct leads to cases where duplicate const and non-const getters are required:

```cpp
const T* Foo::GetT() const { return t_; }
T* Foo::GetT() { return t_; }
```

If the implementation of GetT() is complex, there's a
[trick to implement the non-const getter in terms of the const one](https://stackoverflow.com/questions/123758/how-do-i-remove-code-duplication-between-similar-const-and-non-const-member-func/123995#123995),
courtesy of _Effective C++_:

```cpp
T* Foo::GetT() { return const_cast<T*>(std::as_const(*this).GetT()); }
```

While this is a mouthful, it does guarantee the implementations won't get out of
sync and no const-incorrectness will occur. And once you've seen it a few times,
it's a recognizable pattern.

This is probably the only case where you should see `const_cast` used to remove
constness.  Its use anywhere else is generally indicative of either "const
broken" code, or a boundary between "const correct" and "const okay" code that
could change to "const broken" at any future time without warning from the
compiler.  Both cases should be fixed.
