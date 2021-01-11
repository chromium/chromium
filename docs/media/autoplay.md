# Autoplay of HTMLMediaElements

Autoplay is the concept of playing media elements without user gesture. The
policy that defines when autoplay is allowed is defined [here](https://www.chromium.org/audio-video/autoplay).
The autoplay logic follows the
[HTML spec](https://html.spec.whatwg.org/multipage/embedded-content.html#media-elements).

There are two ways of initiating autoplay:

* Autoplay by attribute: Setting the `autoplay` attribute on the media element.
  The element will try to autoplay when the `readyState` changes to
  HAVE_ENOUGH_DATA.
* Autoplay by `play()` method: Explicitly calling the `play()` method without
  user gesture.

All the autoplay logic is handled by the AutoplayPolicy class. When the media
element wants to perform some action (like unmute, autoplay by attribute or
`play()` method), it will send a request to AutoplayPolicy, and if the request
is approved, the element can autoplay, otherwise it should be paused. Also the
media element should inform the AutoplayPolicy about relevant changes such as
"the element has been moved to a new document".

## User gesture lock

Each media element has a user gesture lock. If the element is allowed to
autoplay, the lock is initialized as `false`, otherwise it's `true`.

When the element is trying to initate autoplay, we check the gesture lock. If
the lock is `false`, it will be allowed. Otherwise autoplay will be blocked. An
exception is that if the element is a muted video element, the gesture lock
check will be bypassed.

To unlock the gesture lock (make it `false`). The page needs to call play() or
load() on the element when responding to a user gesture.

## Autoplay flowchart

The treatments of autoplay by different methods are different. For autoplay by
attribute, it is:

![Autoplay by attribute](autoplay_by_attribute.png)

This means if autoplay is initiated by attribute, it enters the autoplaying
phase, we play it as long as the video is visible. When the page explicitly
calls `play()`, `pause()`, we leave the autoplaying phase. When the page tries
to unmute the video, we check the gesture lock and pause the video if it is
still `true`.

For autoplay by `play()` method, it is:

![Autoplay by play() method](autoplay_by_play_method.png)

This means if autoplay is initiated by `play()` method, we continue playing the
video as normal `play()`. However if the page tries to unmute the video, we check
the gesture lock and pause the video if it is still `true`.
