# Chrome Accessibility on Android

This document covers some of the technical details of how Chrome
implements its accessibility support on Android.

Chrome plays an important role on Android - not only is it the default
browser, but Chrome powers WebView, which is used by many built-in and
third-party apps to display all sorts of content. Android includes a lightweight
implementation called Chrome Custom Tabs, which is also powered by Chrome.
All of these implementations must be accessible, and the Chrome & Chrome OS Accessibility
team provides the support to make these accessibility through the Android API.

Accessibility on Android is heavily used. There are many apps that hijack the
Android accessibility API to act on the user's behalf (e.g. password managers,
screen clickers, anti-virus software, etc). Because of this, roughly **16%** of all
Android users are running the accessibility code (even if they do not realize it).

As background reading, you should be familiar with
[Android Accessibility](https://developer.android.com/guide/topics/ui/accessibility)
and in particular
[AccessibilityNodeInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo)
objects, [AccessibilityEvent](https://developer.android.com/reference/android/view/accessibility/AccessibilityEvent) and
[AccessibilityNodeProvider](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider).

## WebContentsAccessibility

The main Java class that implements the accessibility protocol in Chrome is
[WebContentsAccessibilityImpl.java](https://cs.chromium.org/chromium/src/content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java). This
class acts as the AccessibilityNodeProvider (see above) for a given tab, and will
provide the virtual tree hierarchy, preform actions on the user's behalf, and
send events to downstream services for changes in the web contents.

This class differs in a few key ways from other platforms. First, it represents
and entire web page, including all frames. The ids in the java code are unique IDs,
not frame-local IDs. They are typically referred to as `virtualViewId` in the code
and Android framework/documentation. Another key difference is the construction of
the native objects for nodes. On most platforms, we create a native object for every
AXNode in a web page, and we implement a bunch of methods on that object that assistive
technology can query. Android is different - it's more lightweight in one way, in that we only
create a native AccessibilityNodeInfo object when specifically requested, when
an Android accessibility service is exploring the virtual tree. In another
sense it's more heavyweight, though, because every time a virtual view is
requested we have to populate it with every possible accessibility attribute,
and there are quite a few.

### WebContentsAccessibilityImpl is "lazy" and "on demand"

Every Tab in the Chrome Android browser will have its own instance of
WebContentsAccessibilityImpl. The WebContentsAccessibilityImpl object is created
using a static Factory method with a parameter of the WebContents object for
that tab. See [constructor](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22protected%20WebContentsAccessibilityImpl%22).
(Note: There are a few exceptions to this pattern, for example when constructing
the object without access to a WebContents instance, such as in the case of PaintPreview.)

Although the WebContentsAccessibilityImpl object has been constructed (and
technically instantiated), it will not do anything until an accessibility service
is active and queries the system. The base class of Java widgets [View.java](https://developer.android.com/reference/android/view/View),
has a method [getAccessibilityNodeProvider](https://developer.android.com/reference/android/view/View#getAccessibilityNodeProvider\(\)). Custom views, such as the [ContentView.java](https://source.chromium.org/chromium/chromium/src/+/main:components/embedder_support/android/java/src/org/chromium/components/embedder_support/view/ContentView.java)
class in Chrome (which holds all the web contents for a single Tab), can override
this method to return an instance of AccessibilityNodeProvider. If an app returns
its own instance of AccessibilityNodeProvider, then AT will leverage this instance
when querying the view hierarchy. The WebContentsAccessibilityImpl acts as Chrome's
custom instance of AccessibilityNodeProvider so that it can serve the virtual view
hierarchy of the web to the native accessibility framework.

The first time that `getAccessibilityNodeProvider` is called by the Android system,
the WebContentsAccessibilityImpl will be initialized. This is why we consider it
"lazy" and "on demand", because although it has technically been constructed and
instantiated, it does not perform any actions until AT triggered its initialization.
See [WebContentsAccessibilityImpl#getAccessibilityNodeProvider](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22public%20AccessibilityNodeProvider%20getAccessibilityNodeProvider%22) and the
associated [onNativeInit](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22protected%20void%20onNativeInit%22) methods. The getAccessibilityNodeProvider method
will only be called when an accessibility service is enabled, and so by lazily
constructing only after this call, we ensure that the accessibility code is not
being leveraged for users without any services enabled.

Once initialized, the WebContentAccessibilityImpl plays a part in handling nearly
all accessibility related code on Android. This object will be where AT sends
actions performed by users, it constructs and serves the virtual view hierarchy,
and it dispatches events to AT for changes in the web contents. The
WebContentsAccessibilityImpl object has the same lifecycle as the Tab for which
it was created, and although it won't fire events or serve anything to downstream
AT if the tab is backgrounded/hidden, the object will continue to exist and will
not be destroyed until the Tab is destroyed/closed.

## AccessibilityNodeInfo

The [AccessibilityNodeInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo)
object is at the core of Android accessibility. This is a rather heavy object
which holds every attribute for a given node (a virtual view element) as defined
by the Android API. (Note: The Android accessibiltiy API has different attributes/standards
than the web or other platforms, so there are many special cases and considerations,
more on that below).

As an AccessibilityNodeProvider, the WebContentsAccessibilityImpl class must
override/implement the [createAccessibilityNodeInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider#createAccessibilityNodeInfo\(int\)) method. This is the
method that the accessibility framework will query on behalf of AT to understand
the current virtual view hierarchy. On other platforms, the native-side code may
contain the entire structure of the web contents in native objects, but on Android
the objects are created "on demand" as requested by the framework, and so they are
typically generated synchronously on-the-fly.

The information to populate the AccessibilityNodeInfo objects is contained in
the accessibility tree in the C++ code in the shared BrowserAccessibilityManager.
For Android there is the usual BrowserAccessibilityManagerAndroid, and the
BrowserAccessibilityAndroid classes, as expected, but there is also an additional
[web\_contents\_accessibility\_android.cc](https://cs.chromium.org/chromium/src/content/browser/accessibility/web_contents_accessibility_android.cc) class.
This class is what allows us to connect the Java-side WebContentsAccessibilityImpl
with the C++ side manager, through the Java Native Interface (JNI).

When [WebContentsAccessibilityImpl#createAccessibilityNodeInfo](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22public%20AccessibilityNodeInfoCompat%20createAccessibilityNodeInfo%22) is called for
a given virtual view (web node), the WebContentsAccessibilityImpl object calls into the native
C++ code through JNI, connecting to `web_contents_accessibility_android.cc`. The
web\_contents\_accessibility\_android object in turn compiles information about the
requested node from BrowserAccessibilityAndroid and BrowserAccessibilityManagerAndroid
and then calls back into WebContentsAccessibilityImpl, again through the JNI, to
send this information back to the Java-side to be populated into the
`AccessibilityNodeInfo` object that is being constructed.

These roundtrips across the JNI come with an inherent cost. It is minuscule, but for
thousands of nodes on a page, each with 25+ attributes, it would be too costly to
make so many trips. However, passing all the attributes in one giant function call
is also not ideal. We try to strike a balance by grouping like attributes together
(e.g. all boolean attributes) into a single JNI trip, and make just a few JNI
trips per AccessibilityNodeInfo object. These trips can be found in the
[WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfo](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/web_contents_accessibility_android.cc?q=%22WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfo%22) method,
which is called from WebContentsAccessibilityImpl#createAccessibilityNodeInfo, and
is the core method for compiling a node's attributes and passing them to the Java-side.

### Java-side caching mechanism

One of the most significant performance optimizations in the Android accessibility
code is the addition of a caching mechanism for the Java-side AccessibilityNodeInfo
objects. The cache is built as a simple `SparseArray` of AccessibilityNodeInfo objects.
We use a SparseArray instead of a HashMap because on Java the HashMap requires
Objects for both the key and value, and ideally we would use the `virtualViewId`
of any given node as the key, and this ID is an int (primitive type) in Java. So the
SparseArray is more light weight and is as efficient as using the HashMap in this
case. The array contains AccessibilityNodeInfo objects at the index of the node's
corresponding `virtualViewId`. If an invalid ID is requested, `null` is returned.

In WebContentsAccessibilityImpl's implementation of createAccessibilityNodeInfo,
the cache is queried first, and if it contains a cached version of an object for
this node, then we update that reference and return it. Otherwise the object is
created anew and added to the cache before returning. The rough outline of the
code is:

```
private SparseArray<AccessibilityNodeInfoCompat> mNodeInfoCache = new SparseArray<>();

@Override
public AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
  if (mNodeInfoCache.get(virtualViewId) != null) {
    cachedNode = mNodeInfoCache.get(virtualViewId);
    // ... update cached node through JNI call ...
    return cachedNode;
  } else {
    // Create new node from scratch
    AccessibilityNodeInfo freshNode = // ... construct node through JNI call.
    mNodeInfoCache.put(virtualViewId, freshNode);
    return freshNode;
  }
}
```

When returning a cached node, there are some fields that we always update. This
requires a call through the JNI, but it still much more efficient than constructing
a full node from scratch. Rather than calling `PopulateAccessibilityNodeInfo`, we
call [WebContentsAccessibilityAndroid::UpdateCachedAccessibilityNodeInfo](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/web_contents_accessibility_android.cc?q=%22WebContentsAccessibilityAndroid::UpdateCachedAccessibilityNodeInfo%22).
This method updates the bounding box for the node so that AT knows where to draw
outlines if needed. (Note: it also technically updates RangeInfo on some nodes to
get around a bug in the Android framework, more on that below.)

We clear nodes from the cache in [BrowserAccessibilityManager::OnNodeWillBeDeleted](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_manager_android.cc?q=%22BrowserAccessibilityManagerAndroid::OnNodeWillBeDeleted%22).
We also clear the parent node of any deleted node so that the AccessibilityNodeInfo
object will receive an updated list of children. We also clear any node that has a focus
change during [FireFocusEvent](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_manager_android.cc?q=%22BrowserAccessibilityManagerAndroid::FireFocusEvent%22).

### Bundle extras / API gaps

Much of the richness of the web cannot be captured by the Android accessibility API,
which is designed from a native (Java) widget perspective. When there is a piece of
information that an AT would like to have access to, but there is no way to include
it through the standard API, we put that info in the AccessibilityNodeInfo's
[Bundle](https://developer.android.com/reference/android/os/Bundle).

The Bundle, accessed through `getExtras()` is a map of key\-value pairs which can
hold any arbitrary data we would like. Some examples of extra information for a node:

- Chrome role
- roleDescription
- targetUrl

We also include information unique to Android, such as a "clickableScore", which is
a rating of how likely an object is to be clickable. The boolean "offscreen" is
used to denote if an element is "visible" to the user, but off screen (see more below).
We include unclipped bounds to give the true bounding boxes of a node if we were
not clipping them to be only onscreen. The full list can be seen in the
[list of constants](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22Constants%20defined%20for%20AccessibilityNodeInfo%20Bundle%20extras%20keys%22)
at the top of WebContentsAccessibilityImpl.java.

### Asynchronously adding "heavy" data

Sometimes apps and downstream services will request we add additional information
to the AccessibilityNodeInfo objects that is too computationally heavy to compute
and include for every node. For these cases, the Android API has a method that
can be called by AT, [addExtraDataToAccessibilityNodeInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider#addExtraDataToAccessibilityNodeInfo\(int,%20android.view.accessibility.AccessibilityNodeInfo,%20java.lang.String,%20android.os.Bundle\)). The method is
part of the AccessibilityNodeProvider, and so WebContentsAccessibilityImpl has
its [own implementation](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22public%20void%20addExtraDataToAccessibilityNodeInfo%22) of this for Chrome. When called with valid arguments,
this will start an asynchronous process to add this extra data to the given
AccessibilityNodeInfo object. The two current implementations of this are to add
text character locations, and raw image data.

### AccessibilityNodeInfoCompat

The Android framework includes a [support library](https://developer.android.com/topic/libraries/support-library) for backwards compatibility.
This is a mechanism that allows Android to add new attributes or methods to their
API while also including backwards compatibility across previously released versions
of Android. The WebContentsAccessibilityImpl class makes heavy use of this to
ensure all features are supported on older versions of Android. This was a recent
change, and the rest of the Chrome code base still uses the non-Compat version of
the accessibility code because there is no use-case yet to switch. To make the
change as minimal as possible, the WebContentsAccessibilityImpl uses the Compat
version internally, but when communicating with other parts of Chrome, it will
unwrap the non-Compat object instead. For this entire document, whenever
AccessibilityNodeInfo is mentioned, technically speaking we are using an
[AccessibilityNodeInfoCompat](https://developer.android.com/reference/androidx/core/view/accessibility/AccessibilityNodeInfoCompat) object, and we use the
[AccessibilityNodeProviderCompat](https://developer.android.com/reference/androidx/core/view/accessibility/AccessibilityNodeProviderCompat) version as well. This library is designed to
be transparent to the end-user though, and for simplicity we generally do not
include the word 'Compat' in documentation, conversation, etc.

## Responding to user actions

As the AccessibilityNodeProvider, the WebContentsAccessibilityImpl is responsible for
responding to user actions that come from downstream AT. It is also responsible
for telling downstream AT of Events coming from the web contents, to ensure that
AT is aware of any changes to the web contents state.

### performAction

One of the most important methods in WebContentsAccessibilityImpl is the
implementation of [performAction](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider#performAction\(int,%20int,%20android.os.Bundle\)).
This method is called by the Android framework on behalf of downstream AT for
any user actions, such as focus changes, clicks, scrolls, etc. For most actions,
we call through the JNI to web\_contents\_accessibility\_android, which will call a
corresponding method in BrowserAccessibilityManager to send this request to the
underlying accessibility code. The performAction method parameters include a
virtualViewId, the action, and a Bundle of args/extras. The Bundle may be null,
but sometimes carries necessary information, such as text that a user is trying
to paste into a field. If the accessibility code is able to successfully act on
this performAction call, it will return `true` to the framework, otherwise `false`.

### AccessibilityEventDispatcher

The other direction of communication is from the accessibility code into the framework
and downstream AT. For this we dispatch an [AccessibilityEvent](https://developer.android.com/reference/android/view/accessibility/AccessibilityEvent).
Often times a call to performAction is paired with one or more AccessibilityEvents
being dispatched in response, but AccessibilityEvents can also be sent without
any user interaction, but instead from updates in the web contents. The AccessibilityEvents
are relatively lightweight, and they are constructed following the same model as
the AccessibilityNodeInfo objects (i.e. calling into web\_contents\_accessibility\_android and
being populated through a series of JNI calls). However, the events can put significant
strain on downstream AT, and this is where another important performance optimization
was added.

Traditionally an app would realize it needs to generate and send an AccessibilityEvent,
it would generate it synchronously and send it on the main thread. The web is more
complicated though, and at times could be generating so many events that downstream
AT is strained. To alleviate this, we have implemented a throttling mechanism,
the [AccessibilityEventDispatcher](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/AccessibilityEventDispatcher.java).
Whenever WebContentsAccessibilityImpl requests to send an AccessibilityEvent, it
first goes through this dispatcher. For most events, they are immediately passed
along to the Android framework (e.g. user clicks, focus changes). Some events are
instead put in a queue and dispatched or dropped based on future user actions.

For events that we wish to throttle, we will immediately send the first event received
of that type. We record the system time of this event. If another event request of
the same type is received within a set time limit, which we call the `throttleDelay`,
then we will wait until to send that event until after the delay.

For example, scroll events can only be sent at most once every 100ms, otherwise we would attempt to send
them on every frame. Consider we have sent a scroll event and recorded the system
time to start a throttle delay. If we try to dispatch another scroll event in that delay
window, it will be queued to release after the delay. If during that throttleDelay period another scroll event
is added to the queue, it will replace the previous event and only the last one added
in the 100ms window is dispatched. The timer would then restart for another 100ms.

The delay times can be specific to a view, or specific to an event. That is, we could
say "only dispatch scroll events every 100ms for any given view", meaning two different nodes
could send scroll events in close succession. Or we can say "only dispatch scroll
events every 100ms, regardless of view", in which case all views trying to send that
event type will enter the same queue.

The event types and their delays can be found [in the constructor](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22Define%20our%20delays%20on%20a%20per%20event%20type%20basis%22)
of the WebContentsAccessibilityImpl, which also includes the construction of the
AccessibilityEventDispatcher.

### Delayed AccessibilityEvent construction

The final performance optimization related to events (that is released on 100% stable),
was to delay the construction of the AccessibilityEvent until it is about to be
dispatched. Implementing the Dispatcher helps a significant amount, but there are
many events that can be dropped, and there is no reason to construct an event until
we are sure it will be dispatched. So we do not construct the AccessibilityEvents
until the moment the Dispatcher has started the request to send the event to the
Android framework.

## Testing on Android

Testing on Android happens through a couple of build targets depending on what it
is that we want to test. Android has tests present in the **content\_browsertests**
target, same as the other platforms, which tests the BrowserAccessibilityManagerAndroid
and BrowserAccessibilityAndroid through the various DumpAccessibilityTreeTests
and DumpAccessibilityEventsTests. However, these tests do not cover the
web\_contents\_accessibility\_android layer, or any of the Java-side code. The
web\_contents\_accessibility\_android object and the associated WebContentsAccessibilityImpl
object are not created for content\_browsertests and require a full browser instance
to be available (or at least the content shell). To handle these types of tests
we must use the **content\_shell\_test\_apk** target, which will run an instance of a
web contents and allow the creation/execution of WebContentsAccessibilityImpl and
the corresponding native object. And finally there is the **chrome\_public\_test\_apk**,
which is used to test the Chrome Android UI, outside the web contents, which is
necessary for testing accessibility features that have a user-facing Android UI, such as
image descriptions, the accessibility settings pages, and page zoom.

### Testing the "missing layer"

The "missing layer" in testing refers to the gap in testing for WebContentsAccessibilityImpl,
and namely web\_contents\_accessibility\_android mentioned above. There are three main
classes we use to test these. They are:

- WebContentsAccessibilityTest

    This test suite is used to test the methods of WebContentsAccessibilityImpl.java. It tests
    the various actions of performAction, construction of AccessibilityEvents, and
    various helper methods we use throughout the code.

- WebContentsAccessibilityTreeTest

    This class is the Java-side equivalent of the DumpAccessibilityTreeTests. This test suite
    opens a given html file (shared with the content\_browsertests), generates an
    AccessibilityNodeInfo tree for the page, and then dumps this tree and compares with
    an expectation file (denoted with the `...-expected-android-external.txt` suffix). We continue
    to keep around the content\_browsertests because a failure in one and not the other
    would provide insight into a potential bug location.

- WebContentsAccessibilityEventsTest

    This class is the Java-side equivalent of the DumpAccessibilityEventsTests. Same as the
    suite above, it shares the same html files as the content\_browsertests, opens them,
    runs the Javascript, and records the AccessibilityEvents that are dispatched to
    downstream AT. There is no Android version of the DumpAccessibilityEventsTests though,
    so these expectation files are suffixed with the usual `...-expected-android.txt`.

When new tests are added for content_browsertests, the associated test should also
be added in WebContentsAccessibility\*Test, and there are PRESUBMIT warnings to
remind developers of this (although they are non-blocking).

### Writing new tests

Adding tests is as easy on Android as it is on the other platforms because the
mechanism is in place and only a single new method needs to be added for the test.

If you are adding a new events test, "example-test.html", you would
first create the html file as normal (content/test/data/accessibility/event/example-test.html),
and add the test to the existing `dump_accessibility_events_browsertests.cc`:

```
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, AccessibilityEventsExampleTest) {
  RunEventTest(FILE_PATH_LITERAL("example-test.html"));
}
```

To include this test on Android, you would add a similar block to the
`WebContentsAccessibilityEventsTest.java` class:

```
@Test
@SmallTest
public void test_exampleTest() {
    performTest("example-test.html", "example-test-expected-android.txt");
}
```

Some tests on Android won't produce any events. For these you do not need to
create an empty file, but can instead make the test line:

```
    performTest("example-test.html", EMPTY_EXPECTATIONS_FILE);
```

The easiest approach is to use the above line, run the tests, and if it fails,
the error message will give you the exact text to add to the
`-expected-android.txt` file. The `-expected-android.txt` file should go in the
same directory as the others (content/test/data/accessibility/event).

For adding a new WebContentsAccessibilityTreeTest, you follow the same method but
include the function in the corresponding Java file.

Note: Writing WebContentsAccessibilityTests is much more involved and there is no
general approach that can be encapsulated here, same with the UI tests. For those
there are many existing examples to reference, or you can reach out to an Android developer.

### Running tests and tracking down flakiness

Running tests for Android can seem a bit daunting because it requires new build
targets, an emulator, and different command-line arguments and syntax. But Android
has a few nifty tricks that don't exist on every platform. For example, Android
tests can be run on repeat indefinitely, but set to break on their first failure. This
is great for tracking down flakiness. It is also possible to use a local repository
to test directly on the build bots, which is great when a test works locally but flakes
or fails during builds. Let's look at some basic examples.

First ensure that you have followed the basic [Android setup](https://chromium.googlesource.com/chromium/src/+/main/docs/android_build_instructions.md) guides and can
successfully build the code. You should not proceed further until you can
successfully run the command:

```
autoninja -C out/Debug chrome_apk
```

One of the most important things to remember when building for unit tests is to use
the `x86` architecture, because most emulators use this. (Note: For running on try
bots however, you'll want `arm64`, more on that below). Your gn args should contain
at least:

```
target_os = "android"
target_cpu = "x86"
```

To run the types of tests mentioned above, you'll build with a command similar to:

```
autoninja -C out/Debug content_shell_test_apk
```

The filtering argument for tests is `-f` rather than the `--gtest_filter` that it is
used with content\_browsertests. So to run an example WebContentsAccessibilityTreeTest test,
you may use a command such as:

```
out/Debug/bin/run_content_shell_test_apk --num_retries=0 -f "*WebContentsAccessibilityTreeTest*testExample"
```

This would look for an x86 phone to deploy to, which should be your emulator. You can
choose to setup an emulator in Android Studio, or you can use some of the emulators
that come pre-built in the repo. [More information here](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/android_emulator.md). In general it is best to run on one of the
newer Android versions, but sometimes the newest is unstable. To specify an
emulator to use, you include the `--avd-config` argument, along with the desired
emulator (see link above for full list). This will run the test without opening a
window, but if you'd like to see an emulator window you can add the `--emulator-window`
argument. The `--repeat=#` argument allows repeats, and if set to `-1` along with
the `--break-on-failure` argument, the test will run repeatedly until it fails once.

Putting this all together, to run the example test with no retries per run, run
repeatedly until failure, on the Android 11 emulator, with a window available, you would
use the command:

```
out/Debug/bin/run_content_shell_test_apk \
    --num_retries=0 \
    --repeat=-1 \
    --break-on-failure \
    --emulator window \
    --avd-config tools/android/avd/proto/android_30_google_apis_x86.textpb \
    -f "*WebContentsAccessibilityTreeTest*testExample"
```

All of this information also applies to the UI tests, which use the target:

```
autoninja -C out/Debug chrome_public_test_apk
```

In this case we would have a similar command to run an ImageDescriptions or Settings
related test:

```
out/Debug/bin/run_chrome_public_test_apk \
    --num_retries=0 \
    --repeat=-1 \
    --break-on-failure \
    --emulator window \
    --avd-config tools/android/avd/proto/android_30_google_apis_x86.textpb \
    -f "*ImageDescriptions*"
```

#### Running on try bots

For more information you should reference the `mb.py` [user guide](https://source.chromium.org/chromium/chromium/src/+/main:tools/mb/docs/user_guide.md).

Note: When running on the trybots, you often need to use `target_cpu = "arm64"`, since
these are actual devices and not emulators.

It is not uncommon when working on the Android accessibility code to have a test that
works fine locally, but consistently fails on the try bots. These can be difficult
to debug, but if you run the test directly on the bots it is easier to gain insights.
This is done using the `mb.py` script. You should first build the target exactly as
outlined above (or mb.py will build it for you), and then you use the `mb.py run` command
to start a test. You provide a series of arguments to specify which properties you
want the try bot to have (e.g. which OS, architecture, etc), and you also can include
arguments for the test apk, same as above. Note: It is recommended to at least provide
the argument for the test filter to save time.

With `mb.py`, you use `-s` to specify swarming, and `-d` to specify dimensions, which
narrow down the choice in try bot. The dimensions are added in the form: `-d dimension_name dimension_value`.
You should specify the `pool` as `chromium.tests`, the `device_os_type` as `userdebug`,
and the `device_os` for whichever Android version you're interested in (e.g. `M`, `N`, `O`, etc).
After specifying all your arguments to `mb.py`, include a `--`, and after this `--`
all further arguments are what is passed to the build target (e.g. content\_shell\_test\_apk).

Putting this all together, to run the same tests as above, in the same way, but
on the Android M try bots, you would use the command:

```
tools/mb/mb.py run -s --no-default-dimensions \
    -d pool chromium.tests \
    -d device_os_type userdebug \
    -d device_os M \
    out/Debug \
    content_shell_test_apk \
    -- \
    --num_retries=0 \
    --repeat=-1 \
    --break-on-failure \
    -f "*WebContentsAccessibilityTreeTest*testExample"
```

## Common Android accessibility "gotchas"

- "name" vs. "text"

    On other platforms, there is a concept of "name", and throughout the accessibility
    code there are references to name. In the Android API, this is referred to as
    "text" and is an attribute in the AccessibilityNodeInfo object. In another platform
    you may `setName()` for a node, the equivalent on Android is `info.setText(text)`.
    In the BrowserAccessibilityAndroid class, the relevant method that provides
    this information is [GetTextContentUTF16](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_android.cc?q=BrowserAccessibilityAndroid::GetTextContentUTF16).

- ShouldExposeValueAsName

    The AccessibilityNodeInfo objects of Android do not have a concept of "value".
    This makes some strange cases for nodes that have both a value and text or
    label, and a challenge for how exactly to expose this data through the API.
    The [ShouldExposeValueAsName](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_android.cc?q=BrowserAccessibilityAndroid::ShouldExposeValueAsName)
    method of BrowserAccessibilityAndroid returns a boolean of whether or not the
    node's value should be returned for its name (i.e. text, see above). If this
    value is false, then we concatenate the value with the node's text and
    return this from GetTextContentUTF16. In the cases where ShouldExposeValueAsName
    is true, we expose only the value in the text attribute, and use the "hint"
    attribute of AccessibilityNodeInfo to expose the rest of the information (text,
    label, description, placeholder).

- stateDescription

    The [stateDescription](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo#setStateDescription\(java.lang.CharSequence\))
    attribute of the AccessibilityNodeInfo objects is a recent addition to the API
    which allows custom text to be added for any node. This text is usually read at
    the end of a node's announcement, and does not replace any other content and is
    purely additional information. We make heavy use of the state description
    to try and capture the richness of the web. For example, the Android API has a
    concept of checkboxes being checked or unchecked, but it does not have the concept
    of 'partially checked' as we have on the web (kMixed). When a checkbox is partially checked,
    we include that information in the stateDescription attribute. For some nodes like
    lists we include a stateDescription of the form "in list, item x of y". The full
    list of stateDescriptions can be found in the BrowserAccessibilityAndroid method
    [GetStateDescription](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_android.cc?q=BrowserAccessibilityAndroid::GetStateDescription).

- CollectionInfo and CollectionItemInfo

    The AccessibilityNodeInfo object has some child objects that do not always
    need to be populated, for example [CollectionInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo.CollectionInfo)
    and [CollectionItemInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo.CollectionItemInfo).
    Collections work differently on Android than other platforms, namely that a given
    node does not carry all necessary information to make a determination about the
    overall collection. As the names might suggest, an item in a collection will have
    the CollectionItemInfo populated, but not CollectionInfo, whereas the container
    that holds all the items of the collection will have a CollectionInfo object but
    not a CollectionItemInfo. When a CollectionItemInfo object is present, it is up
    to the downstream AT to walk up the tree and gather information about the full
    Collection. This information is not included on every node.

    These collections are used for any table-like node on
    Android, such as lists, tables, grids, trees, etc. If a node is not table like or
    an item of a table, then these child objects would be `null`. For this example
    tree, the objects present for each node would be:

    ```
    kGenericContainer - CollectionInfo=null; CollectionItemInfo=null
    kList - CollectionInfo=populated; CollectionItemInfo=null
    ++kListItem - CollectionInfo=null; CollectionItemInfo=populated
    ++kListItem - CollectionInfo=null; CollectionItemInfo=populated
    ++kListItem - CollectionInfo=null; CollectionItemInfo=populated
    ```

- contentInvalid

    The Android accessibility API has a boolean field isContentInvalid, however this
    does not play well with downstream AT, so the Chrome code has some special
    implementation details. The accessibility code reports a page exactly as it is,
    so if a text field is labeled as contentInvalid, we report the same to all platforms.
    There are use-cases where a field may be contentInvalid for each character typed
    until a certain regex is met, e.g. when typing an email, empty or a few characters
    would be reported as contentInvalid. When isContentInvalid is true on a node's
    AccessibilityNodeInfo object, then AT (e.g. TalkBack) will proactively announce
    "Error" or "Content Invalid", which can be jarring and unexpected for the user.
    This announcement happens on any change, so every character typed would make
    this announcement and give a bad user experience. It is the opinion of the Chrome
    accessibility team that this ought to be fixed by TalkBack, and reporting an invalid
    node as invalid is the pedantically correct approach. However, in the spirit of
    giving the best user experience possible, we added two workarounds:
    - The contentInvalid boolean is always false if the number of characters in
      the field is less than [kMinimumCharacterCountForInvalid](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_android.cc?q=kMinimumCharacterCountForInvalid), currently set to 7. This is done at the BrowserAccessibilityAndroid level.
    - The WebContentsAccessibilityImpl includes a further workaround. contentInvalid
      will only be reported for a currently focused node, and it will be reported
      at most once every [CONTENT\_INVALID\_THROTTLE\_DELAY](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=CONTENT_INVALID_THROTTLE_DELAY) seconds, currently set to 4.5s.
      See the [setAccessibilityNodeInfoBooleanAttributes](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=setAccessibilityNodeInfoBooleanAttributes)
      method for the full implementation.

- isVisibleToUser vs. "offscreen"

    The Android accessibility API includes only one boolean for setting whether or
    not a node is "visible", the isVisibleToUser attribute. In general this conflicts with
    the way the accessibility code treats [offscreen](https://source.chromium.org/chromium/chromium/src/+/main:docs/accessibility/browser/offscreen.md).
    The name isVisibleToUser may suggest that it reflects whether or not the node
    is currently visible to the user, but a more apt name would be: isPotentiallyVisibleToUser,
    or isNotProgrammaticallyHidden. Nodes that are scrolled off the screen, and thus
    not visible to the user, must still report true for isVisibleToUser. The main use-case
    for this is for AT to allow navigation by element type. For example, if a user wants to
    navigate by Headings, then an app like TalkBack will only navigate through nodes with a
    true value for isVisibleToUser. If any node offscreen has isVisibleToUser as false,
    then it would effectively remove this navigation option. So, the Chrome Android
    accessibility code reports most nodes as isVisibleToUser, and if the node is actually
    offscreen (not programmatically hidden but scrolled offscreen), then we include a
    Bundle extra boolean, "offscreen" so that downstream AT can differentiate between
    the nodes truly on/off screen.

- RangeInfo, aria-valuetext, and caching

    The [RangeInfo](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo.RangeInfo)
    object is another child object of AccessibilityNodeInfo. Unfortunately this
    object is rather limited in its options, and can only provide float values
    for a min, max, and current value. There is no concept of a text description, or
    steps or step size. This clashes with nodes such as sliders with an aria-valuetext,
    or an indeterminate progress bar, for which we have to add special treatment.
    As a further complication, AccessibilityEvents also require information on range
    values when there is a change in value, however the event only allows integer
    values between 0 and 100 (an integer percentage of the sliders position).
    BrowserAccessibilityAndroid has a method [IsRangeControlWithoutAriaValueText](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_android.cc?q=BrowserAccessibilityAndroid::IsRangeControlWithoutAriaValueText)
    which we use to separate these cases when populating AccessibilityNodeInfo
    objects and AccessibilityEvents (see web\_contents\_accessibility\_android.cc).
    Similar to the Collection related objects above, RangeInfo is `null` for any
    non-range related nodes.

    This RangeInfo object plays a small role in updating the cached AccessibilityNodeInfo
    objects above. There is a small bug in the Android framework (which has been fixed
    on newer versions) which breaks our caching mechanism for range objects. So the
    `UpdateCachedAccessibilityNodeInfo` method also updates the RangeInfo object of a node
    if it has one.

- Leaf nodes and links

    Android has slightly different IsLeaf logic than other platforms, and this can
    cause confusion, especially around links. On Android, links are **never** leafs.
    See [IsLeaf](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/browser_accessibility_android.cc?q=BrowserAccessibilityAndroid::IsLeaf).
    This is for similar reasons to the isVisibleToUser section above. If a link
    were a leaf node, and it were to contain something like a Heading, then AT would
    not be able to traverse that link when navigating by headings because it would only
    see it is a link. For this reason we always expose the entire child structure
    of links.

- Refocusing a node Java-side

    There is a strange bug in Android where objects that are accessibility focused
    sometimes do not visually update their outline. This does not really block any
    user flows per se, but we would ideally have the outlines drawn by AT like TalkBack
    to reflect the correct bounds of the node. There is a simple way to get around
    this bug, which is to remove focus from the node and refocus it again, which
    triggers the underlying Android code necessary to update the bounds. In
    WebContentsAccessibilityImpl we have a method
    [moveAccessibilityFocusToIdAndRefocusIfNeeded](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22void%20moveAccessibilityFocusToIdAndRefocusIfNeeded%22)
    which handles this.

- liveRegions and forced announcements

    There is a boolean for liveRegion in the AccessibilityNodeInfo object that the
    Chrome accessibility code will never set. This is because TalkBack will read
    the entirety of the liveRegion node when there is a change. Instead we force
    a custom announcement with an AccessibilityEvent in the WebContentsAccessibilityImpl's
    [announceLiveRegionText](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22private%20void%20announceLiveRegionText%22)
    method.

- sendAccessibilityEvent vs. requestSendAccessibilityEvent

    In the Android framework, there are two methods for sending AccessibiltiyEvents,
    [sendAccessibilityEvent](https://developer.android.com/reference/android/view/View#sendAccessibilityEvent\(int\)) and
    [requestSendAccessibilityEvent](https://developer.android.com/reference/android/view/ViewParent#requestSendAccessibilityEvent\(android.view.View,%20android.view.accessibility.AccessibilityEvent\)).
    Technically speaking, requestSendAccessibilityEvent will ask the system to
    send an event, but it doesn't have to send it. For all intents and purposes,
    we assume that it is always sent, but as a small detail to keep in mind, this
    is not a guarantee.

- TYPE\_WINDOW\_CONTENT\_CHANGED events

    The TYPE\_WINDOW\_CONTENT\_CHANGED type AccessibilityEvent is used to tell the
    framework that something has changed for the given node. We send these events
    for any change in the web, including scrolling. As a result, this is generally the
    most frequently sent event, and we can often send too many and put strain on
    downstream AT. We have included this event as part of our event throttling in
    the AccessibilityEventDispatcher. We also include a small optimization for a
    given atomic update. If an atomic update sends more than
    [kMaxContentChangedEventsToFire](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/web_contents_accessibility_android.h?q=%22kMaxContentChangedEventsToFire%22)
    events (currently set to 5), then any further events are dropped and a single
    event on the root node is sent instead. This has proven useful for situations such
    as many nodes being toggled visible at once. See [HandleContentChanged](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/web_contents_accessibility_android.cc?q=%22WebContentsAccessibilityAndroid::HandleContentChanged%22)
    in web\_contents\_accessibility\_android.cc.

- Text selection

    Static text selection exists in Android, but not when a service like TalkBack
    is enabled. TalkBack allows for text selection inside an editable text field,
    but not static text inside the web contents. Text selection is also tied to a
    specific node with a start and end index. This means that we cannot select
    text across multiple nodes. Ideally the implementation would allow a start
    and end index on separate nodes, but this work is still in development.

- Touch exploration

    The way touch exploration works on Android is complicated. The process that happens
    during any hover event is:

    1. User begins dragging their finger
    2. Java-side View receives a hover event and passes this through to C++
    3. Accessibility code sends a hit test action to the renderer process
    4. The renderer process fires a HOVER accessibility event on the accessibility
      node at that coordinate
    5. [WebContentsAccessibilityImpl#handleHover](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java?q=%22private%20void%20handleHover%22) is called for that node.
    6. We fire a TYPE\_VIEW\_HOVER\_ENTER AccessibilityEvent on that node.
    7. We fire a TYPE\_VIEW\_HOVER\_EXIT AccessibilityEvent on the previous node.
    8. TalkBack sets accessibility focus to the targeted node.

- WebView and Custom Tabs

    As mentioned at the start of this document, Chrome Android also plays an important
    role by providing the WebView, which is used by any third party apps
    that want to include web content in their app. Android also has a unique feature
    of Custom Tabs, which is a lightweight implementation of Chrome that is somewhere
    between a WebView and a full browser. The WebView and custom tabs must also be accessible,
    and so most of this document applies to them as well. Occasionally there
    will be an edge case or small bug that only happens in WebView, or a feature that
    needs to be turned off only for WebView/Custom tab (e.g. image descriptions). There is a
    WebView and Chrome Custom Tabs test app on the chrome-accessibility appspot page,
    and there are methods on the Java-side that can give signals of whether the current
    instance is a WebView or Chrome custom tab. [Example: isCustomTab](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/tab/java/src/org/chromium/chrome/browser/tab/Tab.java?q=isCustomTab).

## Recent Progress and Features

The Chrome Android accessibility code continues to evolve each quarter. We have
strengthened our testing and the stability of the code, but we also continue to
add new features and improvements. Beyond the usual bug fixes, below is a quick
summary of some features in the pipeline.

### OnDemand AT

We have recently implemented a feature we refer to as "OnDemand AT" for short.
This feature is still rolling out and we intend to eventually have it enabled
on 100% stable by default. The feature modifies the AccessibilityEventDispatcher
that is explained above. If the feature is enabled, then WebContentsAccessibilityImpl
will query the Android system to determine the currently enabled accessibility
services, as well as the types of data they are interested in, namely the types of
AccessibilityEvents they want to know about. When the AccessibilityEventDispatcher
is sent an event to add to its queue or dispatch, if that event type is not in
the list of AccessibilityEvents relevant to currently enabled accessibility services,
the Dispatcher simply drops/ignores the request. Preliminary data shows that this
has created a noticeable improvement for accessibility services that do not require
the entire suite to function.

### AccessibilityPerformanceFiltering

Loosely related to the OnDemand feature above, the "AccessibilityPerformanceFiltering" feature is also
a recent addition to improve overall performance. This feature uses the same
mechanism as OnDemand to query the currently enabled services and the information
they are interested in. AccessibilityPerformanceFiltering then takes this information and uses a different
[AXMode](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_mode.h)
based on the situation. Each AXMode filters out a different level of information
to improve performance while preserving necessary functionality. This effectively does the same thing as
OnDemand, but further left/up-the-chain, giving a more significant performance
improvement. This feature is still rolling out, and it currently only has three
AXModes (basic, form controls only, and complete). As it rolls out and we gather more data
we will potentially add more AXModes in the future.

### AutoDisableAccessibility

The "AutoDisable" accessibility feature has also been ported to Android. This feature
tracks timing between user inputs and accessibility actions to make a determination
of whether or not accessibility services are still required. If they are no longer
needed by the user, then the accessibility code is disabled. Before this feature,
once the code was enabled it would continue to run for the life of the current
browser session. This feature is still being rolled out to stable.


## Accessibility code in the ClankUI

Most of this document is focused on the accessibility code and work as it relates
to the web contents, which is where the Chrome & Chrome OS Accessibility team
focuses most of its work. However, some features require a native UI in the browser
app, outside the web contents. When these features are added, the line between
the accessibility team and the Clank UI team becomes blurred. We traditionally
are the owners of this code, but seek regular guidance and approvals from the
Clank team as the front-end code must conform to the Clank standards.

### AccessibilitySettings

The AccessibilitySettings page is found under the overflow/3-dot menu (Menu\>Settings\>Accessibility).
The page currently contains a slider to change the font scaling of the web contents,
options to force enable zoom, show simplified pages, enable image descriptions (see below),
and live captions.

The main entry point for this code is [here](https://source.chromium.org/chromium/chromium/src/+/main:components/browser_ui/accessibility/android/java/src/org/chromium/components/browser_ui/accessibility/AccessibilitySettings.java).
The code leverages the PreferenceFragment of Android, and so much of the UI and
navigation is available out of the box, and the code is relatively simple in that
it only needs to respond to user actions/changes and pass this information to the
native C++ code.

The settings code is heavily unit tested and stable, so it is rare to have to
work in this area.

### Image Descriptions

The Clank-side code for the image descriptions feature is a bit more involved.
The image descriptions has to track state, determine whether or not to display the
option in the overflow menu, show dialogs, and provide toasts to the user. This
code is mostly controlled by the [ImageDescriptionsController](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/image_descriptions/android/java/src/org/chromium/chrome/browser/image_descriptions/ImageDescriptionsController.java).
The image descriptions feature is written using Clank's 'component' model, and so
almost all the code exists in the directory:
[chrome/browser/image_descriptions](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/image_descriptions/)

(The exception being the few hooks that connect this code to the other parts of the
Clank UI).

The image descriptions code is heavily unit tested and stable, so it is rare to
have to work in this area.

### (Upcoming) Page Zoom

An upcoming feature is the page zoom feature, which will allow a more robust way
to zoom web contents than the currently existing text scaling of AccessibilitySettings
(which will be replaced).

The Clank UI code for this feature has not been developed. More to come.
