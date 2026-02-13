# ActivityResultTracker

## Overview

[`ActivityResultTracker`](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/base/ActivityResultTracker.java)
is a utility within Chromium for Android designed to more robustly handle
results from activities started for a result. It ensures that results are not
lost even if the base Activity is destroyed and recreated by the Android system
while the launched activity is still active. This is useful for flows like
account addition starting an activity from a different app, where Chrome can be
killed by the OS in the background to free memory during the flow.

## How to use

1.  **Implement `ActivityResultTracker.ResultListener`:** Your component that
    needs to receive an activity result should implement the
    `ActivityResultTracker.ResultListener` interface. This interface has two
    methods:
    - `onActivityResult(ActivityResult result, @Nullable Bundle savedInstanceData)`:
      This is where you handle the result. The `savedInstanceData` parameter
      contains the optional data that was saved before starting the activity.
    - `getRestorationKey()`: This should return a `String` that uniquely
      identifies the _purpose_ of the activity launch across activity
      recreations. If multiple indistinguishable instances of your component
      must exist in parallel, they can return the same restoration key - an
      arbitrary instance will be chosen to handle the activity result in case
      the base activity is recreated before the new activity finishes.

    ```java
    class MyComponent implements ActivityResultTracker.ResultListener {
        private final String mRestorationKey;

        MyComponent(String restorationKey) {
            mRestorationKey = restorationKey;
        }

        @Override
        public void onActivityResult(ActivityResult result, @Nullable Bundle savedInstanceData) {
            // Handle the result...
        }

        @Override
        public String getRestorationKey() {
            return mRestorationKey;
        }
    }
    ```

2.  **Get an `ActivityResultTracker` Instance:** Get the tracker instance inside
    the base Activity and pass it to your component.

    ```java
    ActivityResultTracker tracker = getActivityResultTracker();
    ```

3.  **Register the Listener:** Before you start the activity, register your
    activity result listener with the tracker.

    ```java
    MyComponent myComponent = new MyComponent("my_unique_restoration_key");
    tracker.register(myComponent);
    ```

4.  **Start the Activity Using the Tracker:**

    ```java
    tracker.startActivity(myComponent, intent, savedInstanceData);
    ```

    The `savedInstanceData` is optional and can be used to save state that needs
    to be restored after the base activity's recreation.

    **Re-registration is Key:** Components MUST call `tracker.register()` with
    the same restoration key in case the base activity is recreated and ensures
    it happens automatically. (e.g. it should not be triggered by an user
    action) If a pending result for your listener's restoration key already
    exists (e.g., from before the activity was recreated), `onActivityResult`
    will be called immediately.

    If multiple instances of a component were registered using the same
    restoration key and a result matching the restoration key is received:
    - if it happens after the base activity's recreation, an arbitrary listener
      will be called to handle the result, usually the first registered one.
    - if it happens without the base activity being killed and restore, the
      component used to start the new activity will be called to handle the
      result.

5.  **Unregister the listener when closing the UI owning it:**
    ```java
    tracker.unregister(myComponent);
    ```

## The initial problem: lost results on Activity recreation

Chrome mainly relies on
[`IntentRequestTracker`](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/base/IntentRequestTracker.java;l=23;drc=f4b1a5f91e14941349a0319b35f71d3efb7350af)
owned by
[`WindowAndroid`](https://source.chromium.org/chromium/chromium/src/+/main:ui/android/java/src/org/chromium/ui/base/WindowAndroid.java;l=438;drc=f4b1a5f91e14941349a0319b35f71d3efb7350af)
for activity result handling. This system is straightforward to use, but has one
limitation: if the base Activity hosting the requesting component is killed by
the system (e.g., due to memory pressure) and then later recreated when the user
navigated back, the callback to handle the activity result is lost. This means
the result from the external activity is effectively discarded, breaking the
user flow. This was observed to affect a noticeable percentage (~1% in 2024) of
sign-in attempts, for example.

The standard Android
[`ActivityResultRegistry`](http://go/android-dev/training/basics/intents/result)
API does support handling results across Activity recreation. However, its
limitation is that callbacks must be registered unconditionally during the
Activity/Fragment initialization or `onCreate` to catch the in-flight activity
results after recreation. This model doesn't fit well with Chrome's
architecture, where many features are encapsulated in components (like
Coordinators) that might be created on demand, much later than the Activity's
`onCreate`. These components, often deeply nested, are the ones that actually
need to initiate an activity and handle its result.

## The implemented solution: the Android ActivityResultRegistry & result caching

`ActivityResultTracker` offers to bridge this gap. It's hosted by the base
Activity and uses the `ActivityResultRegistry` from the Android SDK to ensure
result reception across Activity death and recreation. The key features are:

1.  **State Persistence:** The tracker saves the restoration keys of all
    in-flight activity requests in the Activity's `onSaveInstanceState` bundle.
2.  **Internal Re-registration:** On Activity recreation, the tracker uses the
    restored keys to re-register internal listeners with the
    `ActivityResultRegistry` during `onCreate`. This ensures the tracker catches
    any result sent back.
3.  **Result Caching:** If a result arrives for a key, and the Chrome component
    that originally made the request hasn't yet been recreated and re-registered
    its specific callback, the `ActivityResultTracker` holds the
    `ActivityResult` and the corresponding restoration key in a temporary cache.
4.  **Delayed Delivery:** When the Chrome component (e.g., Coordinator) is
    eventually recreated and calls `ActivityResultTracker.register()` with its
    restoration key, the tracker checks the cache. If a result is pending for
    that key, it's delivered immediately to the newly provided callback
    instance.

This design allows components to be created at any time and still reliably
receive activity results, even if the main Activity is destroyed in the
background.

Note that the cached results are not persisted with the recreation, so the
components needs to ensure early registration of the result callback upon
recreation, to prevent the result from being lost during a second activity
recreation.

## Lifecycle integration

The implementation of `ActivityResultTracker` (e.g.,
`ActivityResultTrackerImpl`) requires hooks into the Activity lifecycle:
`onSaveInstanceState`, `onRestoreInstanceState`, and `onDestroy`. These are
handled by the base Activity class (currently `ChromeBaseAppCompatActivity`).
