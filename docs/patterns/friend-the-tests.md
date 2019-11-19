# The Friend-the-tests Pattern

The Friend-the-tests pattern involves friending test fixture classes from the
class under test. Chromium has a macro named `FRIEND_TEST_ALL_PREFIXES` that
makes this convenient to do.

**Note**: Friending test classes is usually not the best way to provide testing
access to a class. It makes it far too easy to unintentionally depend on the
class's implementation rather than its interface. It is better to use a proper
testing interface than to friend test classes.

## Use this pattern when:

A test needs simple access to the internals of a class, such as access to
individual fields or methods.

## Don't use this pattern when:

* Many different test classes need access to a class's internals: you will have
  to friend too many things and the class under test will end up coupled with
  too many unit tests.
* You're doing it to test private methods: in general, you should not separately
  test private methods; the correctness of the private methods should be
  observable from the correctness of the public methods, and if a piece of
  private behavior has no effect on the public behavior of a class, it might
  actually be unnecessary.

## Alternatives / See also:

* [TestApi]
* [ForTesting methods]
* ["Test the contract, not the implementation"](https://abseil.io/tips/135)
* [Discussion thread on cxx@](https://groups.google.com/a/chromium.org/d/msg/cxx/AOsKCPCBYhk/RDVLSMRGCgAJ)

## How to use this pattern:

```
class Foo {
 public:
  // ... public API ...

 private:
  FRIEND_TEST_ALL_PREFIXES(FooTest, TestSomeProperty);
  FRIEND_TEST_ALL_PREFIXES(FooTest, TestABehavior);

  // or if every test in the suite needs private access:
  friend class FooTest;
};
```

[fortesting methods]: fortesting-methods.md
[testapi]: testapi.md
