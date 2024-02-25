// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_POST_MESSAGE_SUPPORT_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_POST_MESSAGE_SUPPORT_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
class WebLocalFrame;
}  // namespace blink

namespace extensions {

// Helper class the implements postMessage support using gin/ to enable an
// embedder of MimeHandlerView (in an HTMLPlugInElement) to send messages to
// the extension loaded inside the MimeHandlerViewGuest process. This class is
// owned by its Delegate.
class PostMessageSupport {
 public:
  // Provides source and target messages used for posting messages. It is
  // expected to be alive for the lifetime of PostMessageSupport.
  class Delegate {
   public:
    // Returns the Delegate with source frame |web_local_frame| which will be
    // used for internal uses of postMessage.
    static Delegate* FromWebLocalFrame(blink::WebLocalFrame* web_local_frame);

    Delegate();
    virtual ~Delegate();

    // The source frame which is sending the message. This is the embedder frame
    // for a MimeHandlerViewGuest. Must not return nullptr.
    virtual blink::WebLocalFrame* GetSourceFrame() = 0;

    // The target WebFrame which the message is sent to.
    virtual blink::WebFrame* GetTargetFrame() = 0;
    // Returns true if the Delegate is embedded. Used to track postMessage usage
    // to embedded MimeHandlerView (i.e., created using a plugin element but not
    // as a result of frame navigations to a relevant MimeHandlerView MIME.
    virtual bool IsEmbedded() const = 0;
    // Determines whether the (MimeHandlerView) resource in target frame is
    // accessible from source frame (used for UMA).
    virtual bool IsResourceAccessibleBySource() const = 0;

    PostMessageSupport* post_message_support() {
      return post_message_support_.get();
    }

   private:
    std::unique_ptr<PostMessageSupport> post_message_support_;
  };

  // Returns the first postMessage support found on |web_local_frame|. This is
  // the instance which corresponds to a full page MimeHandlerView.
  static PostMessageSupport* FromWebLocalFrame(
      blink::WebLocalFrame* web_local_frame);

  // |delegate| will take ownership of this class.
  explicit PostMessageSupport(Delegate* delegate);
  ~PostMessageSupport();

  // Returns the gin/ implementation of v8::Object exposing the postMessage API.
  // This is used as the PluginWrapper() for HTMLPlugInElement.
  v8::Local<v8::Object> GetScriptableObject(v8::Isolate* isolate);

  // If |is_active_| the message is sent from source frame to the target frame
  // (provided by |delegate_|). If |!is_active_| the messages are queued to be
  // sent as soon as the PostMessageSupport becomes active.
  void PostJavaScriptMessage(v8::Isolate* isolate,
                             v8::Local<v8::Value> message);
  void PostMessageFromValue(const base::Value& message);

  // Activates the PostMessageSupport. After calling this method all the
  // messages in |message_queue_| are forwarded to the target frame.
  void SetActive();
  bool is_active() const { return is_active_; }

 private:
  PostMessageSupport(const PostMessageSupport&) = delete;
  PostMessageSupport& operator=(const PostMessageSupport&) = delete;

  // The scriptable object that backs the plugin.
  v8::Global<v8::Object> scriptable_object_;

  // Pending postMessage messages that need to be sent to the guest. These are
  // queued while the guest is loading and once it is fully loaded they are
  // delivered so that messages aren't lost.
  std::vector<v8::Global<v8::Value>> pending_messages_;

  // When false, all sent messages are queued up in |message_queue_|. When true,
  // the messages are forwarded to the target frame.
  bool is_active_ = false;

  const raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<PostMessageSupport> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_MIME_HANDLER_VIEW_POST_MESSAGE_SUPPORT_H_
