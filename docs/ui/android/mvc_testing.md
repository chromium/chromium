# So, you want to test MVC...

## Overview
This document is intended to go over the best practices of testing the various MVC components.

If you are new to MVC, read the intro guide here: [So, you want to MVC...](mvc_architecture_tutorial.md)


## Testing principles
Keep your tests small and isolated to just the component you are testing.

Keep the number of large integration tests to a minimum (e.g. testing the full coordinator together).

Test each element of your component independently (where possible).  The Mediator should be testable without a View, View without the Mediator.

### General Best Practices

assertTrue/assertFalse should only be used for methods that return booleans.  Here are some examples of the bad usage of assertTrue and their most descriptive and useful counterparts:
  * assertTrue(foo == null);
    * assertIsNull(foo);
  * assertTrue(“test”.equals(foo));
    * assertEquals(“test”, foo);
  * assertTrue(foo.endsWith(“blah”));
    * assertThat(foo, endsWith(“blah”));
    * Look at [Hamcrest Matchers](http://hamcrest.org/JavaHamcrest/javadoc/1.3/org/hamcrest/Matchers.html) for more magical assertion help


## Testing your Mediator
The core business logic of your component should live within your mediator.

Ideally, the Mediator should be testable using Robolectric on the host machine (TODO: link to general Robolectric/Junit test guide).

### Best practices for testability for your Mediator:

 * Pass in all dependencies via the constructor
    * This allows you to mock any necessary external components
 * Do not add native dependencies in the Mediator directly.
    * Put all of your native calls in a separate file that allows you to mock out the JNI calls without spinning up all of native.
 * Your mediator should not need to be extended, spied or mocked for testing.  If you find yourself needing to extend the Mediator to stub out a method, you should move the dependency to something that can be passed in.

### Good examples of Mediator/Test pairs:

 * [TabListMediator](/chrome/android/features/tab_ui/java/src/org/chromium/chrome/browser/tasks/tab_management/TabListMediator.java) / [TabListMediatorUnitTest](/chrome/android/features/tab_ui/junit/src/org/chromium/chrome/browser/tasks/tab_management/TabListMediatorUnitTest.java)
 * [TabSwitcherMediator](/chrome/android/features/tab_ui/java/src/org/chromium/chrome/browser/tasks/tab_management/TabSwitcherMediator.java) / [TabSwitcherMediatorUnitTest](/chrome/android/features/tab_ui/junit/src/org/chromium/chrome/browser/tasks/tab_management/TabSwitcherMediatorUnitTest.java)


## Testing your View

The View/ViewBinder should be tested as an independent unit.

### Best practices for testability for your View:

 * Tests for UI should be based on the [DummyUiActivity](/chrome/test/android/javatests/src/org/chromium/chrome/test/ui/DummyUiActivity.java).  This activity does not have any dependencies on the Chrome browser and allows you to test your UI in isolation.  This ensures you are not competing against other Chrome tasks and can write a test that is much faster and less flaky than legacy UI instrumentation tests.
 * When adding tests for your UI component, add a [RenderTest](/chrome/test/android/javatests/src/org/chromium/chrome/test/util/RENDER_TESTS.md) to ensure the UI is consistent from release to release unless you explicitly changed it.

### Useful helpers / Links

 * To test a single View/ViewBinder normally used in a RecyclerView, this PropertyModelChangeProcessor.ViewBinder implementation, [TestRecyclerViewSimpleViewBinder.java](/chrome/android/features/tab_ui/javatests/src/org/chromium/chrome/browser/tasks/tab_management/TestRecyclerViewSimpleViewBinder.java), allows to test individual UI element without needing to build a full RecyclerView.

### Good examples of View/Test pairs:
 * [TabGridViewBinder](/chrome/android/features/tab_ui/java/src/org/chromium/chrome/browser/tasks/tab_management/TabGridViewBinder.java), [TabStripViewBinder](/chrome/android/features/tab_ui/java/src/org/chromium/chrome/browser/tasks/tab_management/TabStripViewBinder.java) / [TabListViewHolderTest](/chrome/android/features/tab_ui/javatests/src/org/chromium/chrome/browser/tasks/tab_management/TabListViewHolderTest.java)

