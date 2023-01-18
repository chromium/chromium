## Inversion of Control

"Inversion of control" is a design pattern used to allow users of a framework
or library (often called clients) to customize the behavior of the framework.

### Our Example

Examples in this document will be given by extending or modifying this example
API, which is hopefully self-explanatory:

```cpp
class StringKVStore {
 public:
  StringKVStore();
  virtual ~StringKVStore();

  using KeyPredicate = base::RepeatingCallback<bool(const string&)>;

  void Put(const string& key, const string& value);
  void Remove(const string& key);
  void Clear();

  string Get(const string& key) const;
  set<string> GetKeys() const;
  set<string> GetKeysMatching(const KeyPredicate& predicate) const;

  void SaveToPersistentStore();
};
```

### What is inversion of control?

Normally, client code calls into the library to do operations, so control flows
from a high-level class to a low-level class:

```cpp
void YourFunction() {
  // GetKeys() calls into the StringKVStore library
  for (const auto& key : kv_store_.GetKeys()) {
    ...
  }
}
```

In "inverted" control flow, the library calls back into your code after you
call into it, so control flows back from a low-level class to a high-level
class:

```cpp
bool IsKeyInteresting(const string& key) { ... }

void YourFunction() {
  StringKVStore::KeyPredicate predicate =
      base::BindRepeating(&IsKeyInteresting);
  // GetKeysMatching() calls into the StringKVStore library, but it calls
  // back into IsKeyInteresting defined in this file!
  for (const auto& key : kv_store_.GetKeysMatching(predicate)) {
    ...
  }
}
```

It is also often inverted in the Chromium dependency sense. For example, in
Chromium, code in //content can't call, link against, or generally be aware of
code in //chrome - the normal flow of data and control is only in one direction,
from //chrome "down" to //content. When //content calls back into //chrome, that
is an inversion of control.

Abstractly, inversion of control is defined by a low-level class defining an
interface that a high-level class supplies an implementation of. In the example
fragment given above, `StringKVStore` defines an interface called
`StringKVStore::KeyPredicate`, and `YourFunction` supplies an implementation of
that interface - namely the bound instance of `IsKeyInteresting`. This allows
the low-level class to use functionality of the high-level class without being
aware of the specific high-level class's existence, or a high-level class to
plug logic into a low-level class.

There are a few main ways this is done in Chromium:

* Callbacks
* Observers
* Listeners
* Delegates

**Inversion of control should not be your first resort. It is sometimes useful
for solving specific problems, but in general it is overused in Chromium.**

### Callbacks

Callbacks are one of the simplest ways to do inversion of control, and often are
all you need. Callbacks can be used to split out part of the framework's logic
into the client, like so:

```cpp
void StringKVStore::GetKeysMatching(const KeyPredicate& predicate) {
  set<string> keys;
  for (const auto& key : internal_keys()) {
    if (predicate.Run(key))
      keys.insert(key);
  }
  return keys;
}
```

where `predicate` was supplied by the client of
`StringKVStore::GetKeysMatching`. They can also be used for the framework
library to notify clients of events, like so:

```cpp
void StringKVStore::Put(const string& key, const string& value) {
  ...
  // In real code you would use CallbackList instead, but for explanatory
  // purposes:
  for (const auto& callback : key_changed_callbacks_)
    callback.Run(...);
}
```

making use of [Subscription].

Callbacks can also be used to supply an implementation of something deliberately
omitted, like so:

```cpp
class StringKVStore {
  using SaveCallback = base::RepeatingCallback<void(string, string)>;
  void SaveToPersistentStore(const SaveCallback& callback);
};
```

### Observers

An "observer" receives notifications of events happening on an object. For
example, an interface like this might exist:

```cpp
class StringKVStore::Observer {
 public:
  virtual void OnKeyChanged(StringKVStore* store,
                            const string& key,
                            const string& from_value,
                            const string& to_value) {}
  virtual void OnKeyRemoved(StringKVStore* store,
                            const string& key,
                            const string& old_value) {}
  ...
}
```

and then on the StringKVStore class:

```cpp
class StringKVStore {
 public:
  ...
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
}
```

So an example of a `StringKVStore::Observer` might be:

```cpp
class HelloKeyWatcher : public StringKVStore::Observer {
 public:
  void OnKeyChanged(StringKVStore* store,
                    const string& key,
                    const string& from_value,
                    const string& to_value) override {
    if (key == "hello")
      ++hello_changes_;
  }
  void OnKeyRemoved(StringKVStore* store,
                    const string& key,
                    const string& old_value) override {
    if (key == "hello")
      hello_changes_ = 0;
  }
}
```

where the `StringKVStore` arranges to call the relevant method on each
`StringKVStore::Observer` that has been added to it whenever a matching event
happens.

Use an observer when:

* More than one client may care to listen to events happening
* Clients passively observe, but do not modify, the state of the framework
  object being observed

### Listeners

A listener is an observer that only observes a single type of event. These were
very common in C++ and Java before the introduction of lambdas, but these days
are not as commonly seen, and you probably should not introduce new listeners -
instead, use a plain [Callback].

Here's an example:

```cpp
class StringKVStore::ClearListener {
 public:
  virtual void OnCleared(StringKVStore* store) = 0;
}
```

Use a listener when:

* There is only a single client listener instance at most per framework object
* There is only a single event being listened for

### Delegates

A delegate is responsible for implementing part of the framework that is
deliberately missing. While observers and listeners are generally passive with
respect to the framework object they are attached to, delegates are generally
active.

One very common use of delegates is to allow clients to make policy decisions,
like so:

```cpp
class StringKVStore::Delegate {
 public:
  virtual bool ShouldPersistKey(StringKVStore* store, const string& key);
  virtual bool IsValidValueForKey(StringKVStore* store,
                                  const string& key,
                                  const string& proposed_value);
};
```

Another common use is to allow clients to inject their own subclasses of
framework objects that need to be constructed by the framework, by putting
a factory method on the delegate:

```cpp
class StringKVStore::Delegate {
 public:
  virtual unique_ptr<StringKVStoreBackend>
      CreateBackend(StringKVStore* store);
}
```

And then these might exist:

```cpp
class MemoryBackedStringKVStoreDelegate : public StringKVStore::Delegate;
class DiskBackedStringKVStoreDelegate : public StringKVStore::Delegate;
...
```

Use a delegate when:

* There needs to be logic that happens synchronously with what's happening in
  the framework
* It does not make sense to have a decision made statically per instance of a
  framework object

### Observer vs Listener vs Delegate

If every call to the client could be made asynchronous and the API would still
work fine for your use case, you have an observer or listener, not a delegate.

If there might be multiple interested client objects instead of one, you have an
observer, not a listener or delegate.

If any method on your interface has any return type other than `void`, you have
a delegate, not an observer or listener.

You can think of it this way: an observer or listener interface *notifies* the
observer or listener of a change to a framework object, while a delegate usually
helps *cause* the change to the framework object.

### Callbacks vs Observers/Listeners/Delegates

Callbacks have advantages:

* No separate interface is needed
* Header files for client classes are not cluttered with the interfaces or
  methods from them
* Client methods don't need to use specific names, so the name-collision
  problems above aren't present
* Client methods can be bound (using [Bind]) with any needed state, including
  which object they are attached to, so there is no need to pass the framework
  object of interest back into them
* The handler for an event is placed in object setup, rather than being implicit
  in the presence of a separate method
* They sometimes save creation of "trampoline" methods that simply discard or
  add extra parameters before invoking the real handling logic for an event
* Forwarding event handlers is a lot easier, since callbacks can easily be
  passed around by themselves
* They avoid multiple inheritance

They also have disadvantages:

* They can lead to deeply-nested setup code
* Callback objects are heavyweight (performance and memory wise) compared to
  virtual method calls

### Design Tips

1. Observers should have empty method bodies in the header, rather than having
   their methods as pure virtuals. This has two benefits: client classes can
   implement only the methods for events they care to observe, and it is
   obvious from the header that the base observer methods do not need to be
   called.

2. Similarly, delegates should have sensible base implementations of every
   method whenever this is feasible, so that client classes (subclasses of the
   delegate class) can concern themselves only with the parts that are
   relevant to their use case.

3. When inverting control, always pass the framework object of interest back to
   the observer/listener/delegate; that allows the client, if it wants to, to
   reuse the same object as the observer/listener/delegate for multiple
   framework objects. For example, if ButtonListener (given above) didn't pass
   the button in, the same ButtonListener instance could not be used to listen
   to two buttons simultaneously, since there would be no way to tell which
   button received the click.

4. Large inversion-of-control interfaces should be split into smaller
   interfaces when it makes sense to do so. One notorious Chromium example
   is [WebContentsObserver], which observes dozens of different events.
   Whenever *any* of these events happens, *every* registered
   WebContentsObserver has to be notified, even though virtually none of them
   might care about this specific event. Using smaller interfaces helps with
   this problem and makes the intent of installing a specific observer clearer.

5. The framework class *should not* take ownership of observers or listeners.
   For delegates the decision is less clear, but in general, err on the side of
   not taking ownership of delegates either. It is common to hold raw pointers
   to observers and listeners, and raw or weak pointers to delegates, with
   lifetime issues managed via AddObserver/RemoveObserver or the helper classes
   discussed below.

6. Depending on your application and how widely-used you expect your observer,
   listener, or delegate to be, you should probably use names that are longer
   and more specific than you might otherwise. This is because client classes
   may be implementing multiple inversion-of-control interfaces, so it is
   important that their method names not collide with each other. For example,
   instead of having `PageObserver::OnLoadStarted`, you might have
   `PageObserver::OnPageLoadStarted` to reduce the odds of an unpleasant
   collision with `NetworkRequestObserver::OnLoadStarted` (or similar). Note
   that callbacks entirely avoid this problem.

7. A callback is probably a better fit for what you're trying to do than one
   of the other patterns given above!

### Inversion of Control in Chromium

Some key classes in `//base`:

* [base::ScopedObservation]
* [ObserverList] and [CheckedObserver]
* [Subscription] and [CallbackList]

And some production examples:

* [WebContentsObserver] and [WebContentsDelegate]
* [BrowserListObserver]
* [URLRequestJobFactory::ProtocolHandler]
* [WidgetObserver] and [ViewObserver]

### When Not To Use This Pattern

Inverted control can be harder to reason about, and more expensive at runtime,
than other approaches. In particular, beware of using delegates when static data
would be appropriate. For example, consider this hypothetical interface:

```cpp
class StringKVStore::Delegate {
  virtual bool ShouldSaveAtDestruction() { return true; }
}
```

It should be clear from the naming that this method will only be called once per
StringKVStore instance and that its value cannot meaningfully change within the
lifetime of a given instance; in this case, "should save at destruction" should
instead be a parameter given to StringKVStore directly.

A good rule of thumb is that any method on a delegate that:

* Will only be called once for a given framework object, or
* Has a value that can't meaningfully change for a given framework object, and
* Serves primarily to return that value, rather than doing some other work
  like constructing a helper object

should be a property on the framework object instead of a delegate method.

[Bind]: ../../base/functional/bind.h
[BrowserListObserver]: ../../chrome/browser/ui/browser_list_observer.h
[CallbackList]: ../../base/callback_list.h
[Callback]: ../../base/functional/callback.h
[CheckedObserver]: ../../base/observer_list_types.h
[ObserverList]: ../../base/observer_list.h
[base::ScopedObservation]: ../../base/scoped_observation.h
[Subscription]: ../../base/callback_list.h
[URLRequestJobFactory::ProtocolHandler]: ../../net/url_request/url_request_job_factory.h
[Unretained]: ../../base/functional/bind.h
[ViewObserver]: ../../ui/views/view_observer.h
[WebContentsDelegate]: ../../content/public/browser/web_contents_delegate.h
[WebContentsObserver]: ../../content/public/browser/web_contents_observer.h
[WidgetObserver]: ../../ui/views/widget/widget_observer.h
