// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"

#include "base/callback_helpers.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_portal_activate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window_post_message_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/portal/portal_activate_event.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

using HTMLPortalElementTest = PageTestBase;

// Virtually all operations should bail out before anything else if this
// HTMLPortalElement is not in a document where portals are enabled.
//
// For this test, we currently emulate this by just turning them off everywhere.
// :)
TEST_F(HTMLPortalElementTest, PortalsDisabledInDocument) {
  Document& document = GetDocument();
  auto* portal = MakeGarbageCollected<HTMLPortalElement>(document);
  ScopedPortalsForTest disable_portals(false);
  ASSERT_FALSE(
      RuntimeEnabledFeatures::PortalsEnabled(document.GetExecutionContext()));

  DummyExceptionStateForTesting exception_state;
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  const auto& console_messages = GetPage().GetConsoleMessageStorage();

  portal->activate(script_state,
                   PortalActivateOptions::Create(script_state->GetIsolate()),
                   exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
            exception_state.CodeAs<DOMExceptionCode>());
  exception_state.ClearException();

  portal->postMessage(
      script_state, ScriptValue::CreateNull(script_state->GetIsolate()),
      MakeGarbageCollected<WindowPostMessageOptions>(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
            exception_state.CodeAs<DOMExceptionCode>());
  exception_state.ClearException();

  auto next_console_message = console_messages.size();
  GetDocument().body()->appendChild(portal, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(next_console_message + 1, console_messages.size());
  EXPECT_TRUE(console_messages.at(next_console_message)
                  ->Message()
                  .Contains("was moved to a document"));

  next_console_message = console_messages.size();
  portal->setAttribute(html_names::kSrcAttr, String("http://example.com/"),
                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ(next_console_message + 1, console_messages.size());
  EXPECT_TRUE(console_messages.at(next_console_message)
                  ->Message()
                  .Contains("was moved to a document"));

  mojo::PendingAssociatedRemote<mojom::blink::Portal> portal_remote;
  mojo::PendingAssociatedReceiver<mojom::blink::Portal> portal_receiver =
      portal_remote.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<mojom::blink::PortalClient> client_remote;
  mojo::PendingAssociatedReceiver<mojom::blink::PortalClient> client_receiver =
      client_remote.InitWithNewEndpointAndPassReceiver();

  auto* activate_event = PortalActivateEvent::Create(
      &GetFrame(), PortalToken(), std::move(portal_remote),
      std::move(client_receiver), nullptr, nullptr, base::NullCallback());
  activate_event->adoptPredecessor(exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kNotSupportedError,
            exception_state.CodeAs<DOMExceptionCode>());
  exception_state.ClearException();
}

}  // namespace
}  // namespace blink
