---
name: ios/document-member-declaration-order-for-destruction-safety
description: When declaring class members or iOS test fixtures with destruction dependencies (WebTaskEnvironment, profiles, Mojo endpoints, observers), declare dependent members last and document ordering
rubric: safety
severity: blocking
citations:
  - path: docs/dangling_ptr_guide.md
    heading: Incorrect destruction order
    quote: Recall that destructors destroy class members in the inverse order of their appearance. It is usually possible to resolve destruction order issues by re-ordering member declarations so that members which need to live longer come first. It is important to order members correctly to prevent pre-existing and future UAF in destructors.
  - path: mojo/public/cpp/bindings/README.md
    heading: A Note About Endpoint Lifetime and Callbacks
    quote: Once a `mojo::Remote<T>` is destroyed, it is guaranteed that pending callbacks as well as the connection error handler (if registered) won't be called.
  - path: docs/threading_and_tasks_testing.md
    heading: Task Environments
    quote: There are a rare exceptions, like `base::test::ScopedFeatureList`, that need to outlive the task environment.
  - path: components/viz/README.md
    heading: Testing Viz Features
    quote: As a general rule `base::ScopedFeatureList` should be the first thing initialized in a test fixture to avoid data races and incorrect feature values.
---

**Rule.** When declaring class member variables or Objective-C ivars with
destruction order dependencies, declare referenced dependencies and services
before dependent members (such as pointers, scoped_refptr, or observers
referencing them) so dependent members are destroyed first in
reverse-declaration order. Declare Mojo endpoints (such as mojo::Receiver,
mojo::Remote, mojo::AssociatedReceiver, mojo::AssociatedRemote, and
mojo::ReceiverSet) at the end of member lists to disconnect IPC and cancel
pending callbacks before dependent members are destroyed. When declaration order
is required for safety across raw pointers, references, or observers in
non-obvious production code, document which member must outlive the other in an
explanatory comment. Local RAII guards (such as `base::ScopedClosureRunner`)
follow the same rule: declare the dependency's guard before the dependent's so
that it runs last on scope exit. Additionally, in iOS test fixtures and test
classes, declare member variables in strict lifetime sequence:

1. Foundational test environment and local-state fixtures—such as
   base::test::ScopedFeatureList, web::WebTaskEnvironment (or
   base::test::TaskEnvironment), and IOSChromeScopedTestingLocalState (or
   ScopedTestingLocalState)—must be declared first, with ScopedFeatureList
   before the task environment so feature overrides are set before background
   tasks run and outlive the threads joined during teardown.
2. Owning browser and profile objects—such as TestProfileIOS,
   TestProfileManagerIOS, and TestBrowser—must be declared after environment and
   local-state fixtures.
3. Dependent states (ProfileState\*, FakeProfileState\*, SceneState\*,
   FakeSceneState\*), coordinators, and non-owning references or observers
   (raw_ptr<T>, base::ScopedObservation, observer tokens) must be declared after
   the owning objects they reference.

**Why.** In C++ and Objective-C++, class members and ivars are destroyed in
reverse order of declaration. If dependent members—such as Mojo endpoints,
observer tokens, or objects holding references or pointers to other members—are
declared before their dependencies, the dependencies are destroyed first during
teardown, causing use-after-free, dangling pointer dereferences, or callback
invocations on destroyed state. Mojo endpoints must be destroyed first to cancel
pending callbacks before the objects handling them are destroyed. Similarly, in
test fixtures, if environment fixtures (web::WebTaskEnvironment,
base::test::ScopedFeatureList) or local state (IOSChromeScopedTestingLocalState)
shut down after dependent profiles or browsers, task queues or preference
registries are torn down while profile destructors are still executing.
Declaring owning objects before dependent states (ProfileState\*, SceneState\*)
or raw_ptr observers triggers dangling-pointer crashes during fixture teardown.
Documenting lifetime dependencies prevents accidental reordering during
refactoring.

## Examples

Bad:

```cpp
// Variant 1: Interdependent members without ordering comment.
@implementation PasswordController {
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<PasswordManager> _passwordManager;
}

// Variant 2: Mojo endpoints declared before dependent members.
class ChromeUrlsHandler : public chrome_urls::mojom::PageHandler {
 private:
  mojo::Receiver<chrome_urls::mojom::PageHandler> receiver_;
  raw_ptr<ProfileIOS> profile_;
};

// Variant 3: Test fixture local state and owning profile declared after dependent states.
class FeatureCoordinatorTest : public PlatformTest {
 protected:
  ProfileState* profile_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};
```

Good:

```cpp
// Variant 1: Documented lifetime dependency between members.
@implementation PasswordController {
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  // `PasswordManager` holds a raw_ptr to `PasswordManagerClient`, therefore
  // `_passwordManagerClient` has to outlive `_passwordManager`.
  std::unique_ptr<PasswordManager> _passwordManager;
}

// Variant 2: Mojo endpoints placed at the end to disconnect first.
class ChromeUrlsHandler : public chrome_urls::mojom::PageHandler {
 private:
  raw_ptr<ProfileIOS> profile_;
  mojo::Receiver<chrome_urls::mojom::PageHandler> receiver_;
};

// Variant 3: Test fixture declared in strict lifetime dependency sequence.
class FeatureCoordinatorTest : public PlatformTest {
 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
};
```
