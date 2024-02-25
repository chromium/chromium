// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_

#include "components/guest_view/common/guest_view.mojom.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace extensions {

// Implements custom bindings for the guestViewInternal API.
class GuestViewInternalCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit GuestViewInternalCustomBindings(ScriptContext* context);
  ~GuestViewInternalCustomBindings() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  struct ViewHolder;
  // ResetMapEntry is called as a callback to SetWeak(). It resets the
  // weak view reference held in |view_map_|.
  static void ResetMapEntry(const v8::WeakCallbackInfo<ViewHolder>& data);

  // AttachIframeGuest attaches a GuestView to a provided <iframe> container
  // element. Once attached, the GuestView will participate in layout of the
  // container page and become visible on screen.
  // AttachIframeGuest takes five parameters:
  // |element_instance_id| uniquely identifies a container within the content
  // module is able to host GuestViews.
  // |guest_instance_id| uniquely identifies an unattached GuestView.
  // |attach_params| is typically used to convey the current state of the
  // container element at the time of attachment. These parameters are passed
  // down to the GuestView. The GuestView may use these parameters to update the
  // state of the guest hosted in another process.
  // |contentWindow| is used to identify the RenderFrame of the <iframe>
  // container element.
  // |callback| is an optional callback that is called once attachment is
  // complete. The callback takes in a parameter for the WindowProxy of the
  // guest identified by |guest_instance_id|.
  void AttachIframeGuest(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Takes a window object and returns the associated WebLocalFrame's token.
  void GetFrameToken(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Destroys the GuestViewContainer given an element instance ID in |args|.
  void DestroyContainer(const v8::FunctionCallbackInfo<v8::Value>& args);

  // GetViewFromID takes a view ID, and returns the GuestView element associated
  // with that ID, if one exists. Otherwise, null is returned.
  void GetViewFromID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // RegisterDestructionCallback registers a JavaScript callback function to be
  // called when the guestview's container is destroyed.
  // RegisterDestructionCallback takes in a single paramater, |callback|.
  void RegisterDestructionCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // RegisterView takes in a view ID and a GuestView element, and stores the
  // pair as an entry in |view_map_|. The view can then be retrieved using
  // GetViewFromID.
  void RegisterView(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Runs a JavaScript function with user gesture.
  //
  // This is used to request webview element to enter fullscreen (from the
  // embedder).
  // Note that the guest requesting fullscreen means it has already been
  // triggered by a user gesture and we get to this point if embedder allows
  // the fullscreen request to proceed.
  void RunWithGesture(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Runs a JavaScript function that may use window.customElements.define
  // with allowlisted custom element names.
  void AllowGuestViewElementDefinition(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  guest_view::mojom::GuestViewHost* GetGuestViewHost();

  mojo::AssociatedRemote<guest_view::mojom::GuestViewHost> remote_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_
