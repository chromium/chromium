// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_JS_BRIDGE_TRAITS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_JS_BRIDGE_TRAITS_H_

namespace content {

// Type traits for WebUIJsBridges. The struct should have three
// `using`s. For example:
// ```cpp
// template <>
// struct JsBridgeTraits<content::FooWebUIController> {
//  // The Mojo interface for FooWebUIController's WebUIJsBridge.
//  using Interface = content::mojom::FooJsBridge;
//  // The implementation of the WebUIJsBridge Mojo interface.
//  using Implementation = content::FooJsBridgeImpl;
//  // Helper class used by WebUIBrowserInterfaceBrokerRegistry to
//  // register Interface binder and register WebUI Mojo interfaces.
//  using BinderInitializer = content::FooJsBridgeBinderInitializer;
// };
// ```
template <typename ControllerType>
struct JsBridgeTraits;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_JS_BRIDGE_TRAITS_H_
