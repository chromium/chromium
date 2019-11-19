# Chrome Accessibility on Android

Chrome plays an important role on Android - not only is it the default
browser, but Chrome powers WebView, which is used by many built-in and
third-party apps to display all sorts of content.

This document covers some of the technical details of how Chrome
implements its accessibility support on Android.

As background reading, you should be familiar with
[https://developer.android.com/guide/topics/ui/accessibility](Android Accessibility)
and in particular
[https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeInfo](AccessibilityNodeInfo)
and
[https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider](AccessibilityNodeProvider).

## WebContentsAccessibility

The main Java class that implements the accessibility protocol in Chrome is
[https://cs.chromium.org/chromium/src/content/public/android/java/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityImpl.java](WebContentsAccessibilityImpl.java). It implements the AccessibilityNodeProvider
interface, so a single Android View can be represented by an entire tree
of virtual views. Note that WebContentsAccessibilityImpl represents an
entire web page, including all frames. The ids in the java code are unique IDs,
not frame-local IDs.

On most platforms, we create a native object for every AXNode in a web page,
and we implement a bunch of methods on that object that assistive technology
can query.

Android is different - it's more lightweight in one way, in that we only
create a native AccessibilityNodeInfo when specifically requested, when
an Android accessibility service is exploring the virtual tree. In another
sense it's more heavyweight, though, because every time a virtual view is
requested we have to populate it with every possible accessibility attribute,
and there are quite a few.

## Populating AccessibilityNodeInfo

Populating AccessibilityNodeInfo is a bit complicated for reasons of
Android version compatibility and also code efficiency.

WebContentsAccessibilityImpl.createAccessibilityNodeInfo is the starting
point. That's called by the Android framework when we need to provide the
info about one virtual view (a web node).

We call into C++ code - 
[https://cs.chromium.org/chromium/src/content/browser/accessibility/web_contents_accessibility_android.cc](web_contents_accessibility_android.cc) from
there, because all of the information about the accessibility tree is
using the shared C++ BrowserAccessibilityManager code.

However, the C++ code then calls back into Java in order to set all of the
properties of AccessibilityNodeInfo, because those have to be set in Java.
Each of those methods, like setAccessibilityNodeInfoBooleanAttributes, is
often overridden by an Android-version-specific subclass to take advantage
of newer APIs where available.

Having the Java code query C++ for every individual attribute one at a time
would be too expensive, we'd be going across the JNI boundary too many times.
That's why it's structured the way it is now.

## Touch Exploration

The way touch exploration works on Android is complicated:

* When the user taps or drags their finger, our View gets a hover event.
* Accessibility code sends a hit test action to the renderer process
* The renderer process fires a HOVER accessibility event on the accessibility
  node at that coordinate
* WebContentsAccessibilityImpl.handleHover is called with that node. We fire
  an Android TYPE_VIEW_HOVER_ENTER event on that node and a
  TYPE_VIEW_HOVER_EXIT event on the previous node.
* Finally, TalkBack sets accessibility focus to that node.
