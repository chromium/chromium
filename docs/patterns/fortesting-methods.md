# The ForTesting Methods Pattern

The ForTesting pattern involves creating public helper methods on a class to
provide access to test-only functionality, and giving them a specific type of
name (`XForTesting` or `XForTest`) to provide a signal at the call site that they
are not intended for regular use.

## Use this pattern when:

You have a widely-used object that you need to expose a small amount of test
functionality on.

## Don't use this pattern when:

* You have lots of different ForTesting methods: consider the [TestApi] pattern
  instead.
* Only a small set of test cases need access: consider the [friend the tests]
  pattern instead, to avoid polluting the public API.

## Alternatives / See also:

* [Friend the tests]
* [TestApi]
* [ForTesting in the style guide](../../styleguide/c++/c++.md)

## How to use this pattern:

```
class Foo {
 public:
  // ... regular public API ...

  void DoStuffForTesting();
};
```

The ForTesting suffix indicates to code reviewers that the method should not be
called in production code. There is a very similar antipattern in which the
suffix is missing:

```
class Foo {
 public:
  // ... regular public API ...

  // Do not call! Only for testing!
  void DoStuff();
};
```

[testapi]: testapi.md
[friend the tests]: friend-the-tests.md
