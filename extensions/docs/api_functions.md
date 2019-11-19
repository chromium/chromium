# Extension API Functions

[TOC]

## Summary

Extension APIs are implemented in terms of functions, events, types, and
properties.  For many APIs, extension functions provide the majority of the
functionality and complexity.  This document describes the typical extension API
function control flow and best practices for implementing new API functions.

## Extension Function Flow

Most extension functions are asynchronous, with the majority of the work
happening in the browser process.  Extensions will provide a callback to the
function to be invoked when the function is complete.  The control flow for most
extension functions is as follows:
* JavaScript in the renderer process (typically, though not always, an extension
  context in an extension's process) calls an API (e.g.,
  `chrome.tabs.create()`).
* Renderer bindings validate the call (see also the
  [bindings documentation](/extensions/renderer/bindings.md), and forward the
  arguments to the browser if the call is valid.
* The extension function behavior is implemented in the browser process, and
  performs the necessary work (e.g., creating a new tab).
* The function replies to the renderer with the result (or error) from running
  the function.  The renderer bindings return the result (or error) to the
  calling extension.

Some functions deviate from this flow - the most common case is with custom
bindings code or renderer-side behavior.  The
[bindings documentation](/extensions/renderer/bindings.md)) describes some
situations in which this happens.

## Extension Function Implementation

Most extension functions are implemented in the browser process.  Each
extension function has a separate class that implements the behavior for that
API function.  A single function instance is created for each invocation of the
function, and is created with information about the call (including arguments,
caller, etc).

### Doing Stuff
An extension API function performs the necessary behavior by overriding the
`ExtensionFunction::Run()` method.  `ExtensionFunction::Run()` returns a
`ResponseAction`, which indicates what should happen next. This can either be a
result to return to the caller (if the function finishes synchronously), an
error (if something went wrong), or a signal to respond later (if the function
will finish asynchronously).  If a function will finish asynchronously, it must
call `ExtensionFunction::Respond()` to deliver the result to the caller.

### Function Lifetime
Extension functions are reference-counted in order to make asynchronous work
easy.  However, they are not automatically collected when the calling renderer,
or even the associated profile, is shut down.  Extension function
implementations should properly handle these cases if they can potentially
outlive objects they depend on, such as the renderer or profile.

### Function Registration
Extension functions are registered by their name, which is used to route a
request from the renderer to the appropriate function implementation.  They
also use a unique enumeration, which is used in various histograms.

### Example Implementation
Below is an example implementation of a simple Extension API function,
`gizmo.frobulate`. An extension would call this API by calling
`chrome.gizmo.frobulate()`.

#### gizmo\_api.idl
The first step is to define the function in the API schema. See the
[schema documentation](/chrome/common/extensions/api/schemas.md) for more
information.

```
namespace gizmo {
  callback FrobulateCallback = void(DOMString result);

  interface functions {
    // Tells the system to frobulate.
    // |cycles|: The number of cycles for which to frobulate.
    // |callback|: The callback to invoke when the frobulation is done; the
    // result contains the outcome of the frobulation.
    static void frobulate(long cycles,
                          optional FrobulateCallback callback);
  };
};
```

#### gizmo\_api.h
Next, we define the function in C++, starting with the header file.

```
GizmoFrobulateFunction : public ExtensionFunction {
 public:
  // This declares the extension function and initiates the mapping between the
  // string name to the C++ class as well as the histogram value.
  DECLARE_EXTENSION_FUNCTION("gizmo.frobulate", GIZMO_FROBULATE)

  GizmoFrobulateFunction();

 private:
  ~GizmoFrobulateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(GizmoFrobulateFunction);
};
```

#### gizmo\_api.cc
Finally, the meat of the implementation is in the .cc file.

```
GizmoFrobulateFunction:: GizmoFrobulateFunction() = default;
GizmoFrobulateFunction::~ GizmoFrobulateFunction() = default;

ExtensionFunction::ResponseAction GizmoFrobulateFunction::Run() {
  // We can create a typed struct of the arguments from the generated code.
  std::unique_ptr<api::gizmo::Frobulate::Params> params(
      api::gizmo::Frobulate::Params::Create(*args_));

  // EXTENSION_FUNCTION_VALIDATE() is used to assert things that should only
  // ever be true, and should have already been enforced. This equates to a
  // DCHECK in debug builds and a BadMessage() (which kills the calling
  // renderer) in release builds. We use it here because we know that the
  // C++ type conversion should always succeed, since it should have been
  // validated by the renderer bindings.
  EXTENSION_FUNCTION_VALIDATE(params);

  int max_cycles = GetMaxCyclesFromPrefs();

  if (params->cycles > max_cycles) {
    // The bindings can only validate parameters in certain (static) ways, so
    // runtime constraints (such as a user value for the maximum number of
    // frobulation cycles) must be validated by hand.
    // Returned error values are exposed to the extension on the
    // chrome.runtime.lastError property in the callback.
    return RespondNow(
        Error(StringPrintf("Cannot frobulate for more than %d cycles.",
                           max_cycles)));
  }

  std::string frobulate_result = GetFrobulator()->Frobulate(params->cycles);
  // The frobulation succeeded, so return the result to the extension. Note that
  // even though the function finished synchronously in C++, the extension still
  // sees this as an asynchronous function, because the IPC between the
  // renderer and the browser is asynchronous.
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(std::move(frobulate_result))));
}
```

If the `gizmo.frobulate()` function implementation had needed to respond
asynchronously from the browser process, it is fairly straightforward to
implement.

```
ExtensionFunction::ResponseAction GizmoFrobulateFunction::Run() {
  std::unique_ptr<api::gizmo::Frobulate::Params> params(
      api::gizmo::Frobulate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int max_cycles = GetMaxCyclesFromPrefs();
  if (params->cycles > max_cycles) {
    return RespondNow(
        Error(StringPrintf("Cannot frobulate for more than %d cycles.",
                           max_cycles)));
  }

  GetFrobulator()->Frobulate(
      params->cycles,
      // Note that |this| is refcounted, so binding automatically adds a
      // reference.
      base::Bind(&GizmoFrobulateFunction::OnFrobulated, this));

  // Note: Since we are returning RespondLater() here, it is required that
  // Frobulate() did not call the callback synchronously (in which case,
  // Respond() would have already been called).
  return RespondLater();
}

void GizmoFrobulateFunction::OnFrobulated(const std::string& frobulate_result) {
  Respond(OneArgument(std::make_unique<base::Value>(frobulate_result)));
}
```

## Do's and Don't's

### Do
* Consider edge cases.  The bindings should properly validate that the
  parameters passed to the extension function match those specified in the
  schema, but will not validate beyond that.  Consider whether there are any
  restrictions that need to be validated prior to using the input from
  extensions.  For instance, checking that a tabId provided by an extension
  corresponds to an existing tab.

* Account for all possible callers. Extension APIs can be called from different
  contexts - some can be called from WebUI, others from normal web pages, others
  from content scripts. The ExtensionFunction::extension() variable thus can
  be null in certain cases, and should only be assumed to be valid if the
  API is restricted to being called from an extension context.

* Certain functions can be throttled, in case they are too expensive. Consider
  if this is appropriate for the function.

### Don't
* Return both a value and an error.  If something fails, only an error should be
  returned, which will set the value of chrome.runtime.lastError.  Only return a
  value if the call succeeded.

* Fail without setting an error.  Errors are important for developers to
  diagnose what went wrong.  Be descriptive enough to be helpful, but concise.
  For instance, an error might be "No tab found with ID '42'.".

* Use deprecated variants.  The ExtensionFunction implementation has been
  around for a long time, and has undergone various iterations.  There are a
  number of deprecated aspects.  Please don't use them.  These include
  ChromeAsyncExtensionFunction(), GetCurrentBrowser(), and others.

* Use the EXTENSION\_FUNCTION\_VALIDATE() if there is any valid way the
  condition could be false. For instance, it is valid to use it to assert that
  conversions that should have been checked by the renderer succeed, since
  otherwise the API should never have been triggered. However, it should not be
  used to validate assertions that are enforced elsewhere, such as validating
  that a tab ID is valid.
