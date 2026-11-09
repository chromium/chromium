---
name: ios/add-unit-tests-for-new-mediators-and-model-classes
description: When a CL adds or changes the logic of a mediator or a model class (browser agent, tab helper, or keyed service), add or extend a matching *_unittest.mm in the same CL
rubric: testing
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Testing Best Practices
    quote: 'Unit tests (`*_unittest.mm`): Required for mediators and model classes (tab helpers, browser agents, services), and recommended for view controllers with non-trivial state or layout logic.'
---

**Rule.** When a CL introduces a new mediator, browser agent, tab helper, or
keyed service in `//ios`, or adds non-trivial logic or a bug fix to an existing
one, the same CL must add or extend a `*_unittest.mm` that exercises that class
and wire it into the relevant `unit_tests` target. Inject fake or test model
services and mock protocols through public initializers and properties rather
than exposing private implementation details. Before flagging, check the CL's
modified files and the class's directory for a matching `*_unittest.mm` (for
example `foo_mediator_unittest.mm` next to `foo_mediator.mm`) that covers the
new or changed code; flag only when none does. Do not flag trivial changes such
as renames, comment edits, or plumbing a parameter through.

**Why.** Mediators and model classes hold the business logic of a feature and
are the cheapest layer to test hermetically; EarlGrey tests are slower, flakier,
and do not cover the edge cases a unit test can. Landing these classes without
tests leaves regressions to be found by users. See
`ios/best_practices.md#testing-best-practices` and
`ios/architecture/mediators.md#unit-testing-mediators`.

## Examples

Bad:

```cpp
// ios/chrome/browser/foo/coordinator/foo_mediator.mm (no matching
// foo_mediator_unittest.mm in the CL or the directory):
@implementation FooMediator

- (void)didSelectItemWithID:(NSString*)itemID {
  _fooService->MarkItemRead(base::SysNSStringToUTF8(itemID));
  [self.consumer setUnreadCount:_fooService->GetUnreadCount()];
}

@end
```

Good:

```cpp
// ios/chrome/browser/foo/coordinator/foo_mediator.mm:
@implementation FooMediator

- (void)didSelectItemWithID:(NSString*)itemID {
  _fooService->MarkItemRead(base::SysNSStringToUTF8(itemID));
  [self.consumer setUnreadCount:_fooService->GetUnreadCount()];
}

@end

// ios/chrome/browser/foo/coordinator/foo_mediator_unittest.mm:
// Tests that selecting an item marks it read and updates the unread count.
TEST_F(FooMediatorTest, DidSelectItemMarksItemRead) {
  [mediator_ didSelectItemWithID:@"item1"];
  EXPECT_TRUE(fake_foo_service_->IsItemRead("item1"));
  EXPECT_EQ(0, consumer_.unreadCount);
}
```
