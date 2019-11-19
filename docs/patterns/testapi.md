# The TestApi Pattern

The TestApi pattern involves creating a helper class to provide access to
test-only functionality on a commonly-used class.

## Use this pattern when:

You have a widely-used object that you need to expose test functionality on, but
you want to be confident that this test functionality is not used outside tests.

## Don't use this pattern when:

* The commonly-used class only needs "test access" from its own tests or
  closely-related tests; in this case, simply [friend the tests].
* Only a handful of simple test-access methods are needed, like trivial setters;
  in this case, the [ForTesting methods] pattern is lighter-weight.

## Alternatives / See also:

* [Friend the tests]
* [ForTesting methods]

## How to use this pattern:

`//foo/commonly_used.h`:
```

class CommonlyUsed {
 public:
  // ... big public API ...

 private:
  friend class CommonlyUsedTestApi;

  // Any private methods to be used by CommonlyUsedTestApi, possibly using the
  // [passkey pattern](passkey.md).
};
```

`//foo/commonly_used_test_api.h`:
```
class CommonlyUsedTestApi {
 public:
  CommonlyUsedTestApi(CommonlyUsed* thing);
  void DoTestStuff(...);

  // or maybe just:
  static void DoTestStuff(CommonlyUsed* thing, ...);

  // and maybe also:
  std::unique_ptr<CommonlyUsed> CreateTestCommonlyUsed(...);
};
```

And then client code can do:
```
  CommonlyUsedTestApi(commonly_used).DoTestStuff(...);
```

Then only link `commonly_used_test_api.{cc,h}` in test targets, so these methods
cannot accidentally be used in production code. This way, CommonlyUsed is not
polluted with code paths that are only used in tests, and code that has "test
access" to CommonlyUsed is very easy to find. Also, coupling between
CommonlyUsed and tests that exercise its test functionality is reduced.

Note that this pattern should be used only judiciously - an extensive TestApi is
often a sign that a class is doing too much or has too much internal state, or
is simply un-ergonomic to use and therefore requires extensive setup. Also, if
many different tests need access to test-only functionality, that may indicate a
gap in the class under test's public API.

[friend the tests]: friend-the-tests.md
[fortesting methods]: fortesting-methods.md
