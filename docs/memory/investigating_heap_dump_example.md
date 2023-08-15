# Example Investigation of a Heap Dump

This document describes the steps taken to investigate a real memory leak
discovered by heap profiling in the wild. For investigators less familiar with
the code base, `Navigating the Stack Trace` should be enough information to
determine the relevant component, and to forward the bug to a component OWNER.

## Understanding the heap dump summary

The opening comment of [Issue
834033](https://bugs.chromium.org/p/chromium/issues/detail?id=834033) contains a
heap dump summary. The highlights are:

* 315723 calls to malloc without corresponding call to free.
* 806MB of memory.
* The common stacktrace for all 315723 allocations.

Usually, anything that uses over 10MB of memory is a red flag. With the
exception of large image resources, most code in Chrome should use much less
than 10MB. Anything that has over 100k allocations is also a red flag.

### Navigating the Stack Trace - Detailed Breakdown

Let's take a look at the stack trace:

```
profiling::(anonymous namespace)::HookAlloc(base::allocator::AllocatorDispatch const*, unsigned long, void*)
base::allocator::MallocZoneFunctionsToReplaceDefault()::$_1::__invoke(_malloc_zone_t*, unsigned long)
<???>
<???>
base::allocator::UncheckedMallocMac(unsigned long, void**)
sk_malloc_flags(unsigned long, unsigned int)
SkMallocPixelRef::MakeAllocate(SkImageInfo const&, unsigned long)
SkBitmap::tryAllocPixels(SkImageInfo const&, unsigned long)
IPC::ParamTraits<SkBitmap>::Read(base::Pickle const*, base::PickleIterator*, SkBitmap*)
ExtensionAction::ParseIconFromCanvasDictionary(base::DictionaryValue const&, gfx::ImageSkia*)
extensions::ExtensionActionSetIconFunction::RunExtensionAction()
extensions::ExtensionActionFunction::Run()
ExtensionFunction::RunWithValidation()
extensions::ExtensionFunctionDispatcher::DispatchWithCallbackInternal(ExtensionHostMsg_Request_Params const&, content::RenderFrameHost*, int, base::RepeatingCallback<void (ExtensionFunction::ResponseType, base::ListValue const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, extensions::functions::HistogramValue)> const&)
extensions::ExtensionFunctionDispatcher::Dispatch(ExtensionHostMsg_Request_Params const&, content::RenderFrameHost*, int)
bool IPC::MessageT<ExtensionHostMsg_Request_Meta, std::__1::tuple<ExtensionHostMsg_Request_Params>, void>::Dispatch<extensions::ExtensionWebContentsObserver, extensions::ExtensionWebContentsObserver, content::RenderFrameHost, void (extensions::ExtensionWebContentsObserver::*)(content::RenderFrameHost*, ExtensionHostMsg_Request_Params const&)>(IPC::Message const*, extensions::ExtensionWebContentsObserver*, extensions::ExtensionWebContentsObserver*, content::RenderFrameHost*, void (extensions::ExtensionWebContentsObserver::*)(content::RenderFrameHost*, ExtensionHostMsg_Request_Params const&))
extensions::ExtensionWebContentsObserver::OnMessageReceived(IPC::Message const&, content::RenderFrameHost*)
extensions::ChromeExtensionWebContentsObserver::OnMessageReceived(IPC::Message const&, content::RenderFrameHost*)
content::WebContentsImpl::OnMessageReceived(content::RenderFrameHostImpl*, IPC::Message const&)
content::RenderFrameHostImpl::OnMessageReceived(IPC::Message const&)
IPC::ChannelProxy::Context::OnDispatchMessage(IPC::Message const&)
base::debug::TaskAnnotator::RunTask(char const*, base::PendingTask*)
base::MessageLoop::RunTask(base::PendingTask*)
base::MessageLoop::DoWork()
base::MessagePumpCFRunLoopBase::RunWork()
base::apple::CallWithEHFrame(void () block_pointer)
base::MessagePumpCFRunLoopBase::RunWorkSource(void*)
<???>
<???>
<???>
<???>
<???>
<???>
<???>
<???>
<???>
__71-[BrowserCrApplication nextEventMatchingMask:untilDate:inMode:dequeue:]_block_invoke
base::apple::CallWithEHFrame(void () block_pointer)
-[BrowserCrApplication nextEventMatchingMask:untilDate:inMode:dequeue:]
<???>
base::MessagePumpNSApplication::DoRun(base::MessagePump::Delegate*)
base::MessagePumpCFRunLoopBase::Run(base::MessagePump::Delegate*)
<name omitted>
ChromeBrowserMainParts::MainMessageLoopRun(int*)
content::BrowserMainLoop::RunMainMessageLoopParts()
content::BrowserMainRunnerImpl::Run()
content::BrowserMain(content::MainFunctionParams)
content::ContentMainRunnerImpl::Run()
service_manager::Main(service_manager::MainParams const&)
content::ContentMain(content::ContentMainParams const&)
ChromeMain
main
<???>
```

The first step is to divide the stack trace into smaller segments to get a
better understanding of what's happening at the time of allocations. The best
way to do this is to segment by name space and/or function prefixes.

```
profiling::(anonymous namespace)::HookAlloc(base::allocator::AllocatorDispatch const*, unsigned long, void*)
base::allocator::MallocZoneFunctionsToReplaceDefault()::$_1::__invoke(_malloc_zone_t*, unsigned long)
<???>
<???>
base::allocator::UncheckedMallocMac(unsigned long, void**)
```

The top of each stack will always contain some `base` and/or `profiling`
code. This is the code responsible for allocating and recording the memory.

```
sk_malloc_flags(unsigned long, unsigned int)
SkMallocPixelRef::MakeAllocate(SkImageInfo const&, unsigned long)
SkBitmap::tryAllocPixels(SkImageInfo const&, unsigned long)
```

Next, we three 3 frames with the prefix `sk`. Searching for
`sk_malloc_flags` on
[codesearch](https://cs.chromium.org/search/?q=sk_malloc_flags&sq=package:chromium&type=cs)
reveals that the component is `third_party/skia`. Looking at the
[README](https://cs.chromium.org/chromium/src/third_party/skia/README) reveals
that Skia is a 2D graphics library.

```
IPC::ParamTraits<SkBitmap>::Read(base::Pickle const*, base::PickleIterator*, SkBitmap*)
```

Next we see a templated function called `Read` in the namespace `IPC`.
`IPC` stands for inter-process communication. This suggests that the
function is responsible for reading an IPC Message, perhaps concerning an
`SkBitmap`.

```
ExtensionAction::ParseIconFromCanvasDictionary(base::DictionaryValue const&, gfx::ImageSkia*)
extensions::ExtensionActionSetIconFunction::RunExtensionAction()
extensions::ExtensionActionFunction::Run()
ExtensionFunction::RunWithValidation()
extensions::ExtensionFunctionDispatcher::DispatchWithCallbackInternal(ExtensionHostMsg_Request_Params const&, content::RenderFrameHost*, int, base::RepeatingCallback<void (ExtensionFunction::ResponseType, base::ListValue const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, extensions::functions::HistogramValue)> const&)
extensions::ExtensionFunctionDispatcher::Dispatch(ExtensionHostMsg_Request_Params const&, content::RenderFrameHost*, int)
bool IPC::MessageT<ExtensionHostMsg_Request_Meta, std::__1::tuple<ExtensionHostMsg_Request_Params>, void>::Dispatch<extensions::ExtensionWebContentsObserver, extensions::ExtensionWebContentsObserver, content::RenderFrameHost, void (extensions::ExtensionWebContentsObserver::*)(content::RenderFrameHost*, ExtensionHostMsg_Request_Params const&)>(IPC::Message const*, extensions::ExtensionWebContentsObserver*, extensions::ExtensionWebContentsObserver*, content::RenderFrameHost*, void (extensions::ExtensionWebContentsObserver::*)(content::RenderFrameHost*, ExtensionHostMsg_Request_Params const&))
extensions::ExtensionWebContentsObserver::OnMessageReceived(IPC::Message const&, content::RenderFrameHost*)
extensions::ChromeExtensionWebContentsObserver::OnMessageReceived(IPC::Message const&, content::RenderFrameHost*)
```

Next, we see many frames with the `extension` prefix. Extensions are exactly
what they sound like - Chrome extensions like AdBlock are used to modify the
behavior of the browser.

```
content::WebContentsImpl::OnMessageReceived(content::RenderFrameHostImpl*, IPC::Message const&)
content::RenderFrameHostImpl::OnMessageReceived(IPC::Message const&)
```

`content` is the name of code that glues together web code [like extensions] and
the rest of Chrome.

```
IPC::ChannelProxy::Context::OnDispatchMessage(IPC::Message const&)
```

More `IPC` code.

```
base::debug::TaskAnnotator::RunTask(char const*, base::PendingTask*)
base::MessageLoop::RunTask(base::PendingTask*)
base::MessageLoop::DoWork()
base::MessagePumpCFRunLoopBase::RunWork()
base::apple::CallWithEHFrame(void () block_pointer)
base::MessagePumpCFRunLoopBase::RunWorkSource(void*)
```

More `base` code. The bottom of most stack traces should go back to
`MessageLoop`, a primitive Chrome construct used to run tasks.

### Navigating the Stack Trace - Summary

* The top and bottom of the stack should generally be the same and are not very
  interesting.
* The prefixes of frames can be used to get a rough idea of the components
  involved.
* Function names can be used to get a rough idea of what's going on.

In this case, extension code is calling `ParseIconFromCanvasDictionary` - so
it's probably trying to parse an icon. This calls into Skia code. Given that
Skia is a 2D drawing library, and the function is `tryAllocPixels`, Skia is
allocating some pixels for the icon. This process is being repeated 315 thousand
times, and the icon is being leaked every time.


## Diving into the code

Now that we have a rough idea of what's happening, let's look at the code for
ParseIconFromCanvasDictionary.

```cpp
bool ExtensionAction::ParseIconFromCanvasDictionary(
    const base::DictionaryValue& dict,
    gfx::ImageSkia* icon) {
  for (base::DictionaryValue::Iterator iter(dict); !iter.IsAtEnd();
       iter.Advance()) {
    std::string binary_string64;
    IPC::Message pickle;
    if (iter.value().is_blob()) {
      pickle = IPC::Message(iter.value().GetBlob().data(),
                            iter.value().GetBlob().size());
    } else if (iter.value().GetAsString(&binary_string64)) {
      std::string binary_string;
      if (!base::Base64Decode(binary_string64, &binary_string))
        return false;
      pickle = IPC::Message(binary_string.c_str(), binary_string.length());
    } else {
      continue;
    }
    base::PickleIterator pickle_iter(pickle);
    SkBitmap bitmap;
    if (!IPC::ReadParam(&pickle, &pickle_iter, &bitmap))
      return false;
    CHECK(!bitmap.isNull());

    // Chrome helpfully scales the provided icon(s), but let's not go overboard.
    const int kActionIconMaxSize = 10 * ActionIconSize();
    if (bitmap.drawsNothing() || bitmap.width() > kActionIconMaxSize)
      continue;

    float scale = static_cast<float>(bitmap.width()) / ActionIconSize();
    icon->AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }
  return true;
}
```

There's a lot going on here, but we can use the information we have to focus.
The leak happens in IPC::ReadParam, so the relevant lines are:

```
SkBitmap bitmap;
if (!IPC::ReadParam(&pickle, &pickle_iter, &bitmap))
  return false;
```

The `IPC` message is being decoded into `bitmap`.

```
 icon->AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
```
Looking at subsequent consumers of `bitmap`, we see that it is being added as a
representation to `icon`. `icon` is an output parameter of this function, so we
have to look at the calling frame,
`ExtensionActionSetIconFunction::RunExtensionAction`.

```
ExtensionFunction::ResponseAction
ExtensionActionSetIconFunction::RunExtensionAction() {
...
    EXTENSION_FUNCTION_VALIDATE(
        ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon));

    if (icon.isNull())
      return RespondNow(Error("Icon invalid."));

    extension_action_->SetIcon(tab_id_, gfx::Image(icon));
...
}
```

In this case, I've already focused on the code that calls
`ParseIconFromCanvasDictionary`. Let's look at `SetIcon`.

```
void ExtensionAction::SetIcon(int tab_id, const gfx::Image& image) {
  SetValue(&icon_, tab_id, image);
}
```

```
template<class T>
void SetValue(std::map<int, T>* map, int tab_id, const T& val) {
  (*map)[tab_id] = val;
}
```

The icon is being added to a map `icon_`, with `tab_id` as the key. Ah ha!
Adding elements to a container [and never removing them] is one of the most
common sources of memory issues.

There are two ways for this memory to be released - the container `icon_` can be
destroyed, or the element can be removed from the container.

`icon_` is a member of `ExtensionAction`, whose documentation reads:
```
// ExtensionAction encapsulates the state of a browser action or page action.
// Instances can have both global and per-tab state. If a property does not have
// a per-tab value, the global value is used instead.
```

This suggests that the lifetime of `icon_` is tied to the lifetime of the
ExtensionAction, which we can guess is tied to the lifetime of the Extension. As
long as the extension stays installed and enabled, `icon_` will not be
destroyed.

Next, we use codesearch to look at all code that removes elements from `icon_`.
The only place that performs removal is

```
void ExtensionAction::ClearAllValuesForTab(int tab_id) {
...
  icon_.erase(tab_id);
...
}
```

This is called by `ExtensionActionAPI::ClearAllValuesForTab`, which is called by
`TabHelper::DidFinishNavigation`. The name of this method suggests that each
time a tab is navigated, the previous tab-specific icon is cleared. However,
that means that if a tab is closed, then the icon is leaked forever.
