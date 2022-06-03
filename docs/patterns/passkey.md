# The Passkey Pattern

For the Chromium implementation of this pattern, see
[//base/types/pass_key.h].

The Passkey pattern is used when you need to expose a subset of a class's
methods to another class in a more granular way than simply friending the other
class. In essence, it involves creating a "passkey" class that can only be
constructed by specific other classes, and requiring an instance of that passkey
class to be passed in when calling methods you wish to restrict the use of. It
is used like this:

```cpp
class Foo {
 public:
  Foo();
  ~Foo();

  void NormalPublicMethod();
  bool AnotherNormalPublicMethod(int a, int b);

  class BarPasskey {
   private:
    friend class Bar;
    BarPasskey() = default;
    ~BarPasskey() = default;
  };

  void HelpBarOut(BarPasskey, ...);
};

...

void Bar::DoStuff() {
  foo->HelpBarOut(Foo::BarPasskey(), ...);
}
```

The private constructor on `Foo::BarPasskey` prevents any class other than `Bar`
from constructing a `Foo::BarPasskey`, which means that:

* Only `Bar` can call those methods
* `Bar` can delegate the ability to call those methods to other
  classes/functions by passing them a `Foo::BarPasskey` instance

This method is effectively free at runtime - a few extra bytes of argument space
are used to pass in the Passkey object.

It is encouraged to leave the `BarPasskey` parameter unnamed to reinforce that it
carries no semantic information and is not actually used for anything.

[//base/types/pass_key.h]: ../../base/types/pass_key.h
