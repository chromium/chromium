# Blink C++ Style Guide

This document is a list of differences from the overall [Chromium Style Guide],
which is in turn a set of differences from the [Google C++ Style Guide]. The
long-term goal is to make both Chromium and Blink style more similar to Google
style over time, so this document aims to highlight areas where Blink style
differs from Google style.

[TOC]

## May use mutable reference arguments

Mutable reference arguments are permitted in Blink, in contrast to Google style.

> Note: This rule is under [discussion](https://groups.google.com/a/chromium.org/d/msg/blink-dev/O7R4YwyPIHc/mJyEyJs-EAAJ).

**OK:**
```c++
// May be passed by mutable reference since |frame| is assumed to be non-null.
FrameLoader::FrameLoader(LocalFrame& frame)
    : frame_(&frame),
      progress_tracker_(ProgressTracker::Create(frame)) {
  // ...
}
```

## Prefer WTF types over STL and base types

See [Blink readme](../../third_party/blink/renderer/README.md#Type-dependencies)
for more details on Blink directories and their type usage.

**Good:**
```c++
  String title;
  Vector<KURL> urls;
  HashMap<int, Deque<RefPtr<SecurityOrigin>>> origins;
```

**Bad:**
```c++
  std::string title;
  std::vector<GURL> urls;
  std::unordered_map<int, std::deque<url::Origin>> origins;
```

When interacting with WTF types, use `wtf_size_t` instead of `size_t`.

## Do not use `new` and `delete`

Object lifetime should not be managed using raw `new` and `delete`. Prefer to
allocate objects instead using `std::make_unique`, `base::MakeRefCounted` or
`blink::MakeGarbageCollected`, depending on the type, and manage their lifetime
using appropriate smart pointers and handles (`std::unique_ptr`, `scoped_refptr`
and strong Blink GC references, respectively). See [How Blink Works](https://docs.google.com/document/d/1aitSOucL0VHZa9Z2vbRJSyAIsAz24kX8LFByQ5xQnUg/edit#heading=h.ekwf97my4bgf)
for more information.

## Naming

### Use `CamelCase` for all function names

All function names should use `CamelCase()`-style names, beginning with an
uppercase letter.

As an exception, method names for web-exposed bindings begin with a lowercase
letter to match JavaScript.

**Good:**
```c++
class Document {
 public:
  // Function names should begin with an uppercase letter.
  virtual void Shutdown();

  // However, web-exposed function names should begin with a lowercase letter.
  LocalDOMWindow* defaultView();

  // ...
};
```

**Bad:**
```c++
class Document {
 public:
  // Though this is a getter, all Blink function names must use camel case.
  LocalFrame* frame() const { return frame_; }

  // ...
};
```

### Precede boolean values with words like “is” and “did”
```c++
bool is_valid;
bool did_send_data;
```

**Bad:**
```c++
bool valid;
bool sent_data;
```

### Precede setters with the word “Set”; use bare words for getters
Precede setters with the word “set”. Prefer bare words for getters. Setter and
getter names should match the names of the variable being accessed/mutated.

If a getter’s name collides with a type name, prefix it with “Get”.

**Good:**
```c++
class FrameTree {
 public:
  // Prefer to use the bare word for getters.
  Frame* FirstChild() const { return first_child_; }
  Frame* LastChild() const { return last_child_; }

  // However, if the type name and function name would conflict, prefix the
  // function name with “Get”.
  Frame* GetFrame() const { return frame_; }

  // ...
};
```

**Bad:**
```c++
class FrameTree {
 public:
  // Should not be prefixed with “Get” since there's no naming conflict.
  Frame* GetFirstChild() const { return first_child_; }
  Frame* GetLastChild() const { return last_child_; }

  // ...
};
```

### Precede getters that return values via out-arguments with the word “Get”
**Good:**
```c++
class RootInlineBox {
 public:
  Node* GetLogicalStartBoxWithNode(InlineBox*&) const;
  // ...
}
```

**Bad:**
```c++
class RootInlineBox {
 public:
  Node* LogicalStartBoxWithNode(InlineBox*&) const;
  // ...
}
```

### May leave obvious parameter names out of function declarations
[Google C++ Style Guide] allows us to leave parameter names out only if
the parameter is not used. In Blink, you may leave obvious parameter
names out of function declarations for historical reason. A good rule of
thumb is if the parameter type name contains the parameter name (without
trailing numbers or pluralization), then the parameter name isn’t needed.

**Good:**
```c++
class Node {
 public:
  Node(TreeScope* tree_scope, ConstructionType construction_type);
  // You may leave them out like:
  // Node(TreeScope*, ConstructionType);

  // The function name makes the meaning of the parameters clear.
  void SetActive(bool);
  void SetDragged(bool);
  void SetHovered(bool);

  // Parameters are not obvious.
  DispatchEventResult DispatchDOMActivateEvent(int detail,
                                               Event& underlying_event);
};
```

**Bad:**
```c++
class Node {
 public:
  // ...

  // Parameters are not obvious.
  DispatchEventResult DispatchDOMActivateEvent(int, Event&);
};
```

## Prefer enums or StrongAliases to bare bools for function parameters
Prefer enums to bools for function parameters if callers are likely to be
passing constants, since named constants are easier to read at the call site.
Alternatively, you can use base::util::StrongAlias<Tag, bool>. An exception to
this rule is a setter function, where the name of the function already makes
clear what the boolean is.

**Good:**
```c++
class FrameLoader {
public:
  enum class CloseType {
    kNotForReload,
    kForReload,
  };

  bool ShouldClose(CloseType) {
    if (type == CloseType::kForReload) {
      ...
    } else {
      DCHECK_EQ(type, CloseType::kNotForReload);
      ...
    }
  }
};

// An named enum value makes it clear what the parameter is for.
if (frame_->Loader().ShouldClose(FrameLoader::CloseType::kNotForReload)) {
  // No need to use enums for boolean setters, since the meaning is clear.
  frame_->SetIsClosing(true);

  // ...
```

**Good:**
```c++
class FrameLoader {
public:
  using ForReload = base::util::StrongAlias<class ForReloadTag, bool>;

  bool ShouldClose(ForReload) {
    // A StrongAlias<_, bool> can be tested like a bool.
    if (for_reload) {
      ...
    } else {
      ...
    }
  }
};

// Using a StrongAlias makes it clear what the parameter is for.
if (frame_->Loader().ShouldClose(FrameLoader::ForReload(false))) {
  // No need to use enums for boolean setters, since the meaning is clear.
  frame_->SetIsClosing(true);

  // ...
```

**Bad:**
```c++
class FrameLoader {
public:
  bool ShouldClose(bool for_reload) {
    if (for_reload) {
      ...
    } else {
      ...
    }
  }
};

// Not obvious what false means here.
if (frame_->Loader().ShouldClose(false)) {
  frame_->SetIsClosing(ClosingState::kTrue);

  // ...
```

## Comments
Please follow the standard [Chromium Documentation Guidelines]. In particular,
most classes should have a class-level comment describing the purpose, while
non-trivial code should have comments describing why the code is written the
way it is. Often, what is apparent when the code is written is not so obvious
a year later.

From [Google C++ Style Guide: Comments]:
> Giving sensible names to types and variables is much better than obscure
> names that must be explained through comments.

### Use `README.md` to document high-level components

Documentation for a related-set of classes and how they interact should be done
with a `README.md` file in the root directory of a component.

### TODO style

Comments for future work should use `TODO` and have a name or bug attached.

From [Google C++ Style Guide: TODO Comments]:

> The person named is not necessarily the person who must fix it.

**Good:**
```c++
// TODO(dcheng): Clean up after the Blink rename is done.
// TODO(https://crbug.com/675877): Clean up after the Blink rename is done.
```

**Bad:**
```c++
// FIXME(dcheng): Clean up after the Blink rename is done.
// FIXME: Clean up after the Blink rename is done.
// TODO: Clean up after the Blink rename is done.
```

[Chromium Style Guide]: c++.md
[Google C++ Style Guide]: https://google.github.io/styleguide/cppguide.html
[Chromium Documentation Guidelines]: ../../docs/documentation_guidelines.md
[Google C++ Style Guide: Comments]: https://google.github.io/styleguide/cppguide.html#Comments
[Google C++ Style Guide: TODO Comments]:https://google.github.io/styleguide/cppguide.html#TODO_Comments
