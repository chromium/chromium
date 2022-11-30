# Simple RecyclerView use in MVC

### Overview
This tutorial briefly describes the use of ```RecyclerViews``` in an MVC component. For the
prerequisites to this tutorial, see [Additional Resources](#Additional-Resources).

Using ```RecyclerViews``` in a basic way in MVC is now very similar to how basic non-recycler
lists work. The general flow is as follows:

* Build and maintain a [```ModelList```][model_list_link].
* Create a [```SimpleRecyclerViewAdapter```][simple_rva_link].
* Register each type of view with the adapter.
* Clean up recycled views if necessary.

Both the ```ModelListAdapter``` and the ```SimpleRecyclerViewAdapter```  implement the
```MVCListAdapter``` interface. That is to say that usage of the two adapters is roughly identical.
The main reason to use one or the other is dependent on the capabilities needed by the views that
each adapter is attached to. Changing from using a ```ListView``` to a ```RecyclerView``` should be
simple once the adapter is implemented.

### Additional Resources
* [Introductory MVC tutorial][mvc_tutorial_link]
* [Simple MVC list tutorial][mvc_list_tutorial_link]

### Instantiating a ```SimpleRecyclerViewAdapter```

A ```SimpleRecyclerViewAdapter``` is instantiated the same way a ```ModelListAdapter``` is; you
only need to provide a handle to the ```ModelList``` for the data the view will display.

### Fast view lookup
Typical usage of ```RecyclerView``` requires the creation of a ```ViewHolder``` to cache the views
that frequently have new content bound to them. The ```SimpleRecyclerViewAdapter``` does away with
this concept.

A generic solution to fast view lookup can be found in
[```ViewLookupCachingFrameLayout```][fast_lookup_frame_layout_link]. This version of a
```FrameLayout``` exposes a method: ```fastFindViewById```. The added method attempts to find the
specified view, and if it exists, it will be cached for faster lookups. See the full documentation
for this class [here][fast_lookup_frame_layout_link].

### Cleaning up a recycled view
In some cases complex views can hold on to expensive resources; the most common cases is holding a
reference to a large bitmap. The ```SimpleRecyclerViewAdapter``` does not explicitly provide a way
to do resource cleanup, but this can still be achieved with ```RecyclerView#RecyclerListener```.

```java
recyclerListener = (holder) -> {
    // Do cleanup for |holder| here. The root view can be accessed via |holder#itemView|
};

// Optionally the listener can be defined in-line below.
mRecyclerView.setRecyclerListener(recyclerListener);

````

[model_list_link]:https://codesearch.chromium.org/chromium/src/ui/android/java/src/org/chromium/ui/modelutil/MVCListAdapter.java?rcl=d22c9731463bad77645cb0f1a928dec7da79bff9&l=38
[simple_rva_link]:https://codesearch.chromium.org/chromium/src/ui/android/java/src/org/chromium/ui/modelutil/SimpleRecyclerViewAdapter.java
[mvc_tutorial_link]:https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_architecture_tutorial.md
[mvc_list_tutorial_link]:https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_simple_list_tutorial.md
[fast_lookup_frame_layout_link]:https://codesearch.chromium.org/chromium/src/ui/android/java/src/org/chromium/ui/widget/ViewLookupCachingFrameLayout.java
