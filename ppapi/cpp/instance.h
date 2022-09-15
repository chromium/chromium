// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INSTANCE_H_
#define PPAPI_CPP_INSTANCE_H_

/// @file
/// This file defines the C++ wrapper for an instance.

#include <map>
#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/view.h"

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

/// The C++ interface to the Pepper API.
namespace pp {

class Graphics2D;
class Graphics3D;
class InputEvent;
class InstanceHandle;
class MessageHandler;
class MessageLoop;
class Rect;
class URLLoader;
class Var;

class Instance {
 public:
  /// Default constructor. Construction of an instance should only be done in
  /// response to a browser request in <code>Module::CreateInstance</code>.
  /// Otherwise, the instance will lack the proper bookkeeping in the browser
  /// and in the C++ wrapper.
  ///
  /// Init() will be called immediately after the constructor. This allows you
  /// to perform initialization tasks that can fail and to report that failure
  /// to the browser.
  explicit Instance(PP_Instance instance);

  /// Destructor. When the instance is removed from the web page,
  /// the <code>pp::Instance</code> object will be deleted. You should never
  /// delete the <code>Instance</code> object yourself since the lifetime is
  /// handled by the C++ wrapper and is controlled by the browser's calls to
  /// the <code>PPP_Instance</code> interface.
  ///
  /// The <code>PP_Instance</code> identifier will still be valid during this
  /// call so the instance can perform cleanup-related tasks. Once this function
  /// returns, the <code>PP_Instance</code> handle will be invalid. This means
  /// that you can't do any asynchronous operations such as network requests or
  /// file writes from this destructor since they will be immediately canceled.
  ///
  /// <strong>Note:</strong> This function may be skipped in certain
  /// call so the instance can perform cleanup-related tasks. Once this function
  /// returns, the <code>PP_Instance</code> handle will be invalid. This means
  /// that you can't do any asynchronous operations such as network requests or
  /// file writes from this destructor since they will be immediately canceled.
  virtual ~Instance();

  /// This function returns the <code>PP_Instance</code> identifying this
  /// object.
  ///
  /// @return A <code>PP_Instance</code> identifying this object.
  PP_Instance pp_instance() const { return pp_instance_; }

  /// Init() initializes this instance with the provided arguments. This
  /// function will be called immediately after the instance object is
  /// constructed.
  ///
  /// @param[in] argc The number of arguments contained in <code>argn</code>
  /// and <code>argv</code>.
  ///
  /// @param[in] argn An array of argument names.  These argument names are
  /// supplied in the \<embed\> tag, for example:
  /// <code>\<embed id="nacl_module" dimensions="2"\></code> will produce two
  /// argument names: "id" and "dimensions".
  ///
  /// @param[in] argv An array of argument values.  These are the values of the
  /// arguments listed in the \<embed\> tag, for example
  /// <code>\<embed id="nacl_module" dimensions="2"\></code> will produce two
  /// argument values: "nacl_module" and "2".  The indices of these values
  /// match the indices of the corresponding names in <code>argn</code>.
  ///
  /// @return true on success. Returning false causes the instance to be
  /// deleted and no other functions to be called.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

  /// @{
  /// @name PPP_Instance methods for the module to override:

  /// DidChangeView() is called when the view information for the Instance
  /// has changed. See the <code>View</code> object for information.
  ///
  /// Most implementations will want to check if the size and user visibility
  /// changed, and either resize themselves or start/stop generating updates.
  ///
  /// You should not call the default implementation. For
  /// backwards-compatibility, it will call the deprecated version of
  /// DidChangeView below.
  virtual void DidChangeView(const View& view);

  /// Deprecated backwards-compatible version of <code>DidChangeView()</code>.
  /// New code should derive from the version that takes a
  /// <code>ViewChanged</code> object rather than this version. This function
  /// is called by the default implementation of the newer
  /// <code>DidChangeView</code> function for source compatibility with older
  /// code.
  ///
  /// A typical implementation will check the size of the <code>position</code>
  /// argument and reallocate the graphics context when a different size is
  /// received. Note that this function will be called for scroll events where
  /// the size doesn't change, so you should always check that the size is
  /// actually different before doing any reallocations.
  ///
  /// @param[in] position The location on the page of the instance. The
  /// position is relative to the top left corner of the viewport, which changes
  /// as the page is scrolled. Generally the size of this value will be used to
  /// create a graphics device, and the position is ignored (most things are
  /// relative to the instance so the absolute position isn't useful in most
  /// cases).
  ///
  /// @param[in] clip The visible region of the instance. This is relative to
  /// the top left of the instance's coordinate system (not the page).  If the
  /// instance is invisible, <code>clip</code> will be (0, 0, 0, 0).
  ///
  /// It's recommended to check for invisible instances and to stop
  /// generating graphics updates in this case to save system resources. It's
  /// not usually worthwhile, however, to generate partial updates according to
  /// the clip when the instance is partially visible. Instead, update the
  /// entire region. The time saved doing partial paints is usually not
  /// significant and it can create artifacts when scrolling (this notification
  /// is sent asynchronously from scrolling so there can be flashes of old
  /// content in the exposed regions).
  virtual void DidChangeView(const Rect& position, const Rect& clip);

  /// DidChangeFocus() is called when an instance has gained or lost focus.
  /// Having focus means that keyboard events will be sent to the instance.
  /// An instance's default condition is that it will not have focus.
  ///
  /// The focus flag takes into account both browser tab and window focus as
  /// well as focus of the plugin element on the page. In order to be deemed
  /// to have focus, the browser window must be topmost, the tab must be
  /// selected in the window, and the instance must be the focused element on
  /// the page.
  ///
  /// <strong>Note:</strong>Clicks on instances will give focus only if you
  /// handle the click event. Return <code>true</code> from
  /// <code>HandleInputEvent</code> in <code>PPP_InputEvent</code> (or use
  /// unfiltered events) to signal that the click event was handled. Otherwise,
  /// the browser will bubble the event and give focus to the element on the
  /// page that actually did end up consuming it. If you're not getting focus,
  /// check to make sure you're either requesting them via
  /// <code>RequestInputEvents()<code> (which implicitly marks all input events
  /// as consumed) or via <code>RequestFilteringInputEvents()</code> and
  /// returning true from your event handler.
  ///
  /// @param[in] has_focus Indicates the new focused state of the instance.
  virtual void DidChangeFocus(bool has_focus);

  /// HandleInputEvent() handles input events from the browser. The default
  /// implementation does nothing and returns false.
  ///
  /// In order to receive input events, you must register for them by calling
  /// RequestInputEvents() or RequestFilteringInputEvents(). By
  /// default, no events are delivered.
  ///
  /// If the event was handled, it will not be forwarded to any default
  /// handlers. If it was not handled, it may be dispatched to a default
  /// handler. So it is important that an instance respond accurately with
  /// whether event propagation should continue.
  ///
  /// Event propagation also controls focus. If you handle an event like a mouse
  /// event, typically the instance will be given focus. Returning false from
  /// a filtered event handler or not registering for an event type means that
  /// the click will be given to a lower part of the page and your instance will
  /// not receive focus. This allows an instance to be partially transparent,
  /// where clicks on the transparent areas will behave like clicks to the
  /// underlying page.
  ///
  /// In general, you should try to keep input event handling short. Especially
  /// for filtered input events, the browser or page may be blocked waiting for
  /// you to respond.
  ///
  /// The caller of this function will maintain a reference to the input event
  /// resource during this call. Unless you take a reference to the resource
  /// to hold it for later, you don't need to release it.
  ///
  /// <strong>Note: </strong>If you're not receiving input events, make sure
  /// you register for the event classes you want by calling
  /// <code>RequestInputEvents</code> or
  /// <code>RequestFilteringInputEvents</code>. If you're still not receiving
  /// keyboard input events, make sure you're returning true (or using a
  /// non-filtered event handler) for mouse events. Otherwise, the instance will
  /// not receive focus and keyboard events will not be sent.
  ///
  /// Refer to <code>RequestInputEvents</code> and
  /// <code>RequestFilteringInputEvents</code> for further information.
  ///
  /// @param[in] event The event to handle.
  ///
  /// @return true if the event was handled, false if not. If you have
  /// registered to filter this class of events by calling
  /// <code>RequestFilteringInputEvents</code>, and you return false,
  /// the event will be forwarded to the page (and eventually the browser)
  /// for the default handling. For non-filtered events, the return value
  /// will be ignored.
  virtual bool HandleInputEvent(const pp::InputEvent& event);

  /// HandleDocumentLoad() is called after Init() for a full-frame
  /// instance that was instantiated based on the MIME type of a DOMWindow
  /// navigation. This situation only applies to modules that are
  /// pre-registered to handle certain MIME types. If you haven't specifically
  /// registered to handle a MIME type or aren't positive this applies to you,
  /// your implementation of this function can just return false.
  ///
  /// The given url_loader corresponds to a <code>URLLoader</code> object that
  /// is already opened. Its response headers may be queried using
  /// GetResponseInfo(). If you want to use the <code>URLLoader</code> to read
  /// data, you will need to save a copy of it or the underlying resource will
  /// be freed when this function returns and the load will be canceled.
  ///
  /// This method returns false if the module cannot handle the data. In
  /// response to this method, the module should call ReadResponseBody() to read
  /// the incoming data.
  ///
  /// @param[in] url_loader An open <code>URLLoader</code> instance.
  ///
  /// @return true if the data was handled, false otherwise.
  virtual bool HandleDocumentLoad(const URLLoader& url_loader);

  /// HandleMessage() is a function that the browser calls when PostMessage()
  /// is invoked on the DOM element for the instance in JavaScript. Note
  /// that PostMessage() in the JavaScript interface is asynchronous, meaning
  /// JavaScript execution will not be blocked while HandleMessage() is
  /// processing the message.
  ///
  /// When converting JavaScript arrays, any object properties whose name
  /// is not an array index are ignored. When passing arrays and objects, the
  /// entire reference graph will be converted and transferred. If the reference
  /// graph has cycles, the message will not be sent and an error will be logged
  /// to the console.
  ///
  /// <strong>Example:</strong>
  ///
  /// The following JavaScript code invokes <code>HandleMessage</code>, passing
  /// the instance on which it was invoked, with <code>message</code> being a
  /// string <code>Var</code> containing "Hello world!"
  ///
  /// @code{.html}
  ///
  /// <body>
  ///   <object id="plugin"
  ///           type="application/x-ppapi-postMessage-example"/>
  ///   <script type="text/javascript">
  ///     document.getElementById('plugin').postMessage("Hello world!");
  ///   </script>
  /// </body>
  ///
  /// @endcode
  ///
  /// Refer to PostMessage() for sending messages to JavaScript.
  ///
  /// @param[in] message A <code>Var</code> which has been converted from a
  /// JavaScript value. JavaScript array/object types are supported from Chrome
  /// M29 onward. All JavaScript values are copied when passing them to the
  /// plugin.
  virtual void HandleMessage(const Var& message);

  /// @}

  /// @{
  /// @name PPB_Instance methods for querying the browser:

  /// BindGraphics() binds the given graphics as the current display surface.
  /// The contents of this device is what will be displayed in the instance's
  /// area on the web page. The device must be a 2D or a 3D device.
  ///
  /// You can pass an <code>is_null()</code> (default constructed) Graphics2D
  /// as the device parameter to unbind all devices from the given instance.
  /// The instance will then appear transparent. Re-binding the same device
  /// will return <code>true</code> and will do nothing.
  ///
  /// Any previously-bound device will be released. It is an error to bind
  /// a device when it is already bound to another instance. If you want
  /// to move a device between instances, first unbind it from the old one, and
  /// then rebind it to the new one.
  ///
  /// Binding a device will invalidate that portion of the web page to flush the
  /// contents of the new device to the screen.
  ///
  /// @param[in] graphics A <code>Graphics2D</code> to bind.
  ///
  /// @return true if bind was successful or false if the device was not the
  /// correct type. On success, a reference to the device will be held by the
  /// instance, so the caller can release its reference if it chooses.
  bool BindGraphics(const Graphics2D& graphics);

  /// Binds the given Graphics3D as the current display surface.
  /// Refer to <code>BindGraphics(const Graphics2D& graphics)</code> for
  /// further information.
  ///
  /// @param[in] graphics A <code>Graphics3D</code> to bind.
  ///
  /// @return true if bind was successful or false if the device was not the
  /// correct type. On success, a reference to the device will be held by the
  /// instance, so the caller can release its reference if it chooses.
  bool BindGraphics(const Graphics3D& graphics);

  /// IsFullFrame() determines if the instance is full-frame (repr).
  /// Such an instance represents the entire document in a frame rather than an
  /// embedded resource. This can happen if the user does a top-level
  /// navigation or the page specifies an iframe to a resource with a MIME
  /// type registered by the module.
  ///
  /// @return true if the instance is full-frame, false if not.
  bool IsFullFrame();

  /// RequestInputEvents() requests that input events corresponding to the
  /// given input events are delivered to the instance.
  ///
  /// By default, no input events are delivered. Call this function with the
  /// classes of events you are interested in to have them be delivered to
  /// the instance. Calling this function will override any previous setting for
  /// each specified class of input events (for example, if you previously
  /// called RequestFilteringInputEvents(), this function will set those events
  /// to non-filtering mode).
  ///
  /// Input events may have high overhead, so you should only request input
  /// events that your plugin will actually handle. For example, the browser may
  /// do optimizations for scroll or touch events that can be processed
  /// substantially faster if it knows there are no non-default receivers for
  /// that message. Requesting that such messages be delivered, even if they are
  /// processed very quickly, may have a noticeable effect on the performance of
  /// the page.
  ///
  /// When requesting input events through this function, the events will be
  /// delivered and <em>not</em> bubbled to the page. This means that even if
  /// you aren't interested in the message, no other parts of the page will get
  /// the message.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  ///   RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  ///   RequestFilteringInputEvents(
  ///       PP_INPUTEVENT_CLASS_WHEEL | PP_INPUTEVENT_CLASS_KEYBOARD);
  ///
  /// @endcode
  ///
  /// @param event_classes A combination of flags from
  /// <code>PP_InputEvent_Class</code> that identifies the classes of events
  /// the instance is requesting. The flags are combined by logically ORing
  /// their values.
  ///
  /// @return <code>PP_OK</code> if the operation succeeded,
  /// <code>PP_ERROR_BADARGUMENT</code> if instance is invalid, or
  /// <code>PP_ERROR_NOTSUPPORTED</code> if one of the event class bits were
  /// illegal. In the case of an invalid bit, all valid bits will be applied
  /// and only the illegal bits will be ignored.
  int32_t RequestInputEvents(uint32_t event_classes);

  /// RequestFilteringInputEvents() requests that input events corresponding
  /// to the given input events are delivered to the instance for filtering.
  ///
  /// By default, no input events are delivered. In most cases you would
  /// register to receive events by calling RequestInputEvents(). In some cases,
  /// however, you may wish to filter events such that they can be bubbled up
  /// to the DOM. In this case, register for those classes of events using
  /// this function instead of RequestInputEvents(). Keyboard events must always
  /// be registered in filtering mode.
  ///
  /// Filtering input events requires significantly more overhead than just
  /// delivering them to the instance. As such, you should only request
  /// filtering in those cases where it's absolutely necessary. The reason is
  /// that it requires the browser to stop and block for the instance to handle
  /// the input event, rather than sending the input event asynchronously. This
  /// can have significant overhead.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  ///
  ///   RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  ///   RequestFilteringInputEvents(
  ///       PP_INPUTEVENT_CLASS_WHEEL | PP_INPUTEVENT_CLASS_KEYBOARD);
  ///
  /// @endcode
  ///
  /// @param event_classes A combination of flags from
  /// <code>PP_InputEvent_Class</code> that identifies the classes of events
  /// the instance is requesting. The flags are combined by logically ORing
  /// their values.
  ///
  /// @return <code>PP_OK</code> if the operation succeeded,
  /// <code>PP_ERROR_BADARGUMENT</code> if instance is invalid, or
  /// <code>PP_ERROR_NOTSUPPORTED</code> if one of the event class bits were
  /// illegal. In the case of an invalid bit, all valid bits will be applied
  /// and only the illegal bits will be ignored.
  int32_t RequestFilteringInputEvents(uint32_t event_classes);

  /// ClearInputEventRequest() requests that input events corresponding to the
  /// given input classes no longer be delivered to the instance.
  ///
  /// By default, no input events are delivered. If you have previously
  /// requested input events using RequestInputEvents() or
  /// RequestFilteringInputEvents(), this function will unregister handling
  /// for the given instance. This will allow greater browser performance for
  /// those events.
  ///
  /// <strong>Note: </strong> You may still get some input events after
  /// clearing the flag if they were dispatched before the request was cleared.
  /// For example, if there are 3 mouse move events waiting to be delivered,
  /// and you clear the mouse event class during the processing of the first
  /// one, you'll still receive the next two. You just won't get more events
  /// generated.
  ///
  /// @param[in] event_classes A combination of flags from
  /// <code>PP_InputEvent_Class</code> that identifies the classes of events the
  /// instance is no longer interested in.
  void ClearInputEventRequest(uint32_t event_classes);

  /// PostMessage() asynchronously invokes any listeners for message events on
  /// the DOM element for the given instance. A call to PostMessage() will
  /// not block while the message is processed.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code{.html}
  ///
  /// <body>
  ///   <object id="plugin"
  ///           type="application/x-ppapi-postMessage-example"/>
  ///   <script type="text/javascript">
  ///     var plugin = document.getElementById('plugin');
  ///     plugin.addEventListener("message",
  ///                             function(message) { alert(message.data); },
  ///                             false);
  ///   </script>
  /// </body>
  ///
  /// @endcode
  ///
  /// The instance then invokes PostMessage() as follows:
  ///
  /// @code
  ///
  ///  PostMessage(pp::Var("Hello world!"));
  ///
  /// @endcode
  ///
  /// The browser will pop-up an alert saying "Hello world!"
  ///
  /// When passing array or dictionary <code>PP_Var</code>s, the entire
  /// reference graph will be converted and transferred. If the reference graph
  /// has cycles, the message will not be sent and an error will be logged to
  /// the console.
  ///
  /// Listeners for message events in JavaScript code will receive an object
  /// conforming to the HTML 5 <code>MessageEvent</code> interface.
  /// Specifically, the value of message will be contained as a property called
  /// data in the received <code>MessageEvent</code>.
  ///
  /// This messaging system is similar to the system used for listening for
  /// messages from Web Workers. Refer to
  /// <code>http://www.whatwg.org/specs/web-workers/current-work/</code> for
  /// further information.
  ///
  /// Refer to HandleMessage() for receiving events from JavaScript.
  ///
  /// @param[in] message A <code>Var</code> containing the data to be sent to
  /// JavaScript. Message can have a numeric, boolean, or string value.
  /// Array/Dictionary types are supported from Chrome M29 onward.
  /// All var types are copied when passing them to JavaScript.
  void PostMessage(const Var& message);

  /// Dev-Channel Only
  ///
  /// Registers a handler for receiving messages from JavaScript. If a handler
  /// is registered this way, it will replace the Instance's HandleMessage
  /// method, and all messages sent from JavaScript via postMessage and
  /// postMessageAndAwaitResponse will be dispatched to
  /// <code>message_handler</code>.
  ///
  /// The function calls will be dispatched via <code>message_loop</code>. This
  /// means that the functions will be invoked on the thread to which
  /// <code>message_loop</code> is attached, when <code>message_loop</code> is
  /// run. It is illegal to pass the main thread message loop;
  /// RegisterMessageHandler will return PP_ERROR_WRONG_THREAD in that case.
  /// If you quit <code>message_loop</code> before calling Unregister(),
  /// the browser will not be able to call functions in the plugin's message
  /// handler any more. That could mean missing some messages or could cause a
  /// leak if you depend on Destroy() to free hander data. So you should,
  /// whenever possible, Unregister() the handler prior to quitting its event
  /// loop.
  ///
  /// Attempting to register a message handler when one is already registered
  /// will cause the current MessageHandler to be unregistered and replaced. In
  /// that case, no messages will be sent to the "default" message handler
  /// (pp::Instance::HandleMessage()). Messages will stop arriving at the prior
  /// message handler and will begin to be dispatched at the new message
  /// handler.
  ///
  /// @param[in] message_handler The plugin-provided object for handling
  /// messages. The instance does not take ownership of the pointer; it is up
  /// to the plugin to ensure that |message_handler| lives until its
  /// WasUnregistered() function is invoked.
  /// @param[in] message_loop Represents the message loop on which
  /// MessageHandler's functions should be invoked.
  /// @return PP_OK on success, or an error from pp_errors.h.
  int32_t RegisterMessageHandler(MessageHandler* message_handler,
                                 const MessageLoop& message_loop);

  /// Unregisters the current message handler for this instance if one is
  /// registered. After this call, the message handler (if one was
  /// registered) will have "WasUnregistered" called on it and will receive no
  /// further messages. After that point, all messages sent from JavaScript
  /// using postMessage() will be dispatched to pp::Instance::HandleMessage()
  /// on the main thread. Attempts to call postMessageAndAwaitResponse() from
  /// JavaScript after that point will fail.
  ///
  /// Attempting to unregister a message handler when none is registered has no
  /// effect.
  void UnregisterMessageHandler();

  /// @}

  /// @{
  /// @name PPB_Console methods for logging to the console:

  /// Logs the given message to the JavaScript console associated with the
  /// given plugin instance with the given logging level. The name of the plugin
  /// issuing the log message will be automatically prepended to the message.
  /// The value may be any type of Var.
  void LogToConsole(PP_LogLevel level, const Var& value);

  /// Logs a message to the console with the given source information rather
  /// than using the internal PPAPI plugin name. The name must be a string var.
  ///
  /// The regular log function will automatically prepend the name of your
  /// plugin to the message as the "source" of the message. Some plugins may
  /// wish to override this. For example, if your plugin is a Python
  /// interpreter, you would want log messages to contain the source .py file
  /// doing the log statement rather than have "python" show up in the console.
  void LogToConsoleWithSource(PP_LogLevel level,
                              const Var& source,
                              const Var& value);

  /// @}

  /// AddPerInstanceObject() associates an instance with an interface,
  /// creating an object.
  ///
  /// Many optional interfaces are associated with a plugin instance. For
  /// example, the find in PPP_Find interface receives updates on a per-instance
  /// basis. This "per-instance" tracking allows such objects to associate
  /// themselves with an instance as "the" handler for that interface name.
  ///
  /// In the case of the find example, the find object registers with its
  /// associated instance in its constructor and unregisters in its destructor.
  /// Then whenever it gets updates with a PP_Instance parameter, it can
  /// map back to the find object corresponding to that given PP_Instance by
  /// calling GetPerInstanceObject.
  ///
  /// This lookup is done on a per-interface-name basis. This means you can
  /// only have one object of a given interface name associated with an
  /// instance.
  ///
  /// If you are adding a handler for an additional interface, be sure to
  /// register with the module (AddPluginInterface) for your interface name to
  /// get the C calls in the first place.
  ///
  /// Refer to RemovePerInstanceObject() and GetPerInstanceObject() for further
  /// information.
  ///
  /// @param[in] interface_name The name of the interface to associate with the
  /// instance
  /// @param[in] object
  void AddPerInstanceObject(const std::string& interface_name, void* object);

  // {PENDING: summarize Remove method here}
  ///
  /// Refer to AddPerInstanceObject() for further information.
  ///
  /// @param[in] interface_name The name of the interface to associate with the
  /// instance
  /// @param[in] object
  void RemovePerInstanceObject(const std::string& interface_name, void* object);

  /// Static version of AddPerInstanceObject that takes an InstanceHandle. As
  /// with all other instance functions, this must only be called on the main
  /// thread.
  static void RemovePerInstanceObject(const InstanceHandle& instance,
                                      const std::string& interface_name,
                                      void* object);

  /// Look up an object previously associated with an instance. Returns NULL
  /// if the instance is invalid or there is no object for the given interface
  /// name on the instance.
  ///
  /// Refer to AddPerInstanceObject() for further information.
  ///
  /// @param[in] instance
  /// @param[in] interface_name The name of the interface to associate with the
  /// instance.
  static void* GetPerInstanceObject(PP_Instance instance,
                                    const std::string& interface_name);

 private:
  PP_Instance pp_instance_;

  typedef std::map<std::string, void*> InterfaceNameToObjectMap;
  InterfaceNameToObjectMap interface_name_to_objects_;
};

}  // namespace pp

#endif  // PPAPI_CPP_INSTANCE_H_
