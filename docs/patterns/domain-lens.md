# The Domain Lens Pattern

The Domain Lens Pattern exposes a specific, more restricted view of a
complicated class, customized to the needs of a specific domain. The name comes
from the metaphor of looking at a class "through a specific lens", which
magnifies parts of the class and hides other parts. This helps decouple clients
from the entire implementation of the complicated class.

Here's an example. Suppose we have a class NetworkRequest:

```c++
class NetworkRequest {
  // ... lots of state ...
  size_t bytes_done() const;
  size_t bytes_to_do() const;
};
```

and suppose that we are implementing a UI component that displays the status of
a download, which is backed by a NetworkRequest. We could implement that like:

```c++
class DownloadView : public NetworkRequestObserver {
 public:
  // Start displaying the status of the given network request.
  DownloadView(NetworkRequest* request);

  // NetworkRequestObserver:
  void OnRequestProgress(NetworkRequest* request) override;
};
```

But there are two problems with this:

1. When testing DownloadView, we need an entire NetworkRequest, and it's not
   clear which parts of the NetworkRequest need to be filled in for DownloadView
   to work;
2. If we are refactoring and adding a different class (say for example
   `DiskFileRequest` for a load from local disk), that class must subclass
   `NetworkRequest` for DownloadView to be usable with it, which usually
   involves a lot of empty methods and stub data.

Instead, we can define a domain-specific lens, containing only the subset of the
data in `NetworkRequest` needed to display download progress, like this:

```c++
struct DownloadProgress {
  base::FilePath destination;
  size_t bytes_done;
  size_t bytes_to_do;
};
```

and two interfaces for receiving updates on that domain-specific lens:

```c++
class DownloadProgressObserver {
 public:
  void OnDownloadProgress(const DownloadProgress& progress);
};

class DownloadProgressProvider {
 public:
  void AddObserver(DownloadProgressObserver* observer);
  void RemoveObserver(DownloadProgressObserver* observer);

  DownloadProgress GetCurrentProgress() const;
};
```

at which point `DownloadView` is:

```c++
class DownloadView : public DownloadProgressObserver {
 public:
  DownloadView(DownloadProgressProvider* provider);
};
```

or perhaps, using a callback rather than an observer class:

```c++
using DownloadProgressCallback =
    base::RepeatingCallback<void(const DownloadProgress& progress)>;

class DownloadProgressProvider {
 public:
  base::CallbackListSubscription RegisterDownloadProgressCallback(
      DownloadProgressCallback callback);
  DownloadProgress GetCurrentProgress();
};

class DownloadView {
 public:
  DownloadView(DownloadProgressProvider* provider);
};
```

## When To Use A Domain Lens

A domain lens makes sense when you have:

* A complicated object with a lot of state
* A well-defined domain or client class that makes use of part of that
  complicated object

A common use of domain lenses is to extract only the data needed to display an
object into a separate type, to decouple the object from its display logic. One
Chromium example of this is [TabRendererData], which encapsulates only the state
of a Tab needed to draw that Tab somewhere, without providing access to the
Tab's underlying webcontents or any methods for mutating it.

Here's another hypothetical, to illustrate this pattern. We have a class
[ProfileAttributesEntry], which contains a lot of different pieces of data about
a Profile. We might define a type and a new method on `ProfileAttributes`,
specifically for drawing the profile "chip" that can replace the app menu icon:

```c++
// Note that this is a separate top-level type from ProfileAttributes. One could
// instead define this as a nested type ProfileAttributes::LensForChip or
// something similar, but then any user of this domain lens would need to have
// the declaration of ProfileAttributes visible as well, which would cause some
// of the coupling we're trying to avoid in the first place.
struct ProfileAttributesForChip {
  gfx::ImageSkia icon;
  std::string name;
  ...
};

ProfileAttributesForChip ProfileAttributes::ForChip() {
  ...
}
```

and then have the profile chip code use a `ProfileAttributesForChip` instead of
a `ProfileAttributes`.

## When Not To Use A Domain Lens

Domain lenses are easy to overuse. In general, any given client of a complicated
class uses only a subset of that class's functionality, but that does not mean
each client should have its own lens - that would cause a lot of extra lens
types and tangle the code needlessly. This pattern always involves writing extra
translation functions and adapters, so it should only be done when the decreased
coupling is worth the extra code needed to implement the lens itself. Clients
that don't have their own domain lens on an object can continue to use the
object itself as usual.

[TabRendererData]: ../../chrome/browser/ui/tabs/tab_renderer_data.h
[ProfileAttributesEntry]: ../../chrome/browser/profiles/profile_attributes_entry.h
