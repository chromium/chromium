---
name: ios/remove-unused-scoped-feature-list-in-tests
rubric: readability
severity: should_fix
---

**Rule.** When authoring, refactoring, or cleaning up tests and test fixtures,
keep the test environment scoped to what the tests exercise. Do not instantiate
`base::test::TaskEnvironment` (or `web::WebTaskEnvironment`) unless the code
under test posts tasks, runs timers, or requires a task runner. Omit unneeded
testing factories (`AddTestingFactory`) and the fakes they install. Scope
feature flags strictly to what tests exercise—deleting unreferenced
`base::test::ScopedFeatureList` members, their headers, and no-op empty calls
like `InitWithFeatures({}, {})` left behind when a flag is removed. Unused
fixture members in general (RAII helpers such as `ScopedKeyWindow`, smart
pointers, mock objects) fall under
`ios/remove-unused-member-variables-and-teardown`.

**Why.** Redundant test setup boilerplate clutters tests and misleads readers
about dependencies or asynchronous behavior: a `TaskEnvironment` suggests the
code under test is asynchronous, a testing factory suggests a service
dependency, and a `ScopedFeatureList` suggests the behavior is flag-dependent.
These helpers have non-trivial constructors, so they escape `-Wunused-variable`
and `-Wunused-private-field` diagnostics while still mutating global state
(feature overrides, task runners, keyed-service factories) and adding execution
overhead to every test in the fixture.

## Examples

Bad:

```cpp
// 1. Empty/unneeded feature override or unreferenced ScopedFeatureList member:
class FooTabHelperTest : public PlatformTest {
  void SetUp() override {
    // Flag removed, leaving no-op empty feature list call!
    feature_list_.InitWithFeatures(/*enabled_features=*/{},
                                   /*disabled_features=*/{});
  }
  base::test::ScopedFeatureList feature_list_;
};

// 2. Testing factory installed although nothing in the test resolves the service:
class FooModelTest : public PlatformTest {
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(BarServiceFactory::GetInstance(),
                              BarServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
  }
  web::WebTaskEnvironment task_environment_;  // Required by TestProfileIOS.
  std::unique_ptr<TestProfileIOS> profile_;
};

// 3. Task environment for purely synchronous code that never posts tasks:
class FooFormatterTest : public PlatformTest {
  base::test::TaskEnvironment task_environment_;
};
```

Good:

```cpp
// 1. Remove ScopedFeatureList and no-op initialization when no flags remain:
class FooTabHelperTest : public PlatformTest {
  void SetUp() override {
    // No ScopedFeatureList initialization needed.
  }
};

// 2. Keep only the factories the test exercises:
class FooModelTest : public PlatformTest {
  void SetUp() override {
    profile_ = TestProfileIOS::Builder().Build();
  }
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// 3. No task environment when nothing under test is asynchronous:
class FooFormatterTest : public PlatformTest {};
```
