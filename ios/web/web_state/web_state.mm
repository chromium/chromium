// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <string_view>

#import "ios/web/public/web_client.h"

namespace web {

WebState::CreateParams::CreateParams(web::BrowserState* browser_state)
    : browser_state(browser_state), created_with_opener(false) {}

WebState::CreateParams::~CreateParams() {}

WebState::OpenURLParams::OpenURLParams(const GURL& url,
                                       const GURL& virtual_url,
                                       const Referrer& referrer,
                                       WindowOpenDisposition disposition,
                                       ui::PageTransition transition,
                                       bool is_renderer_initiated)
    : url(url),
      virtual_url(virtual_url),
      referrer(referrer),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated) {}

WebState::OpenURLParams::OpenURLParams(const GURL& url,
                                       const Referrer& referrer,
                                       WindowOpenDisposition disposition,
                                       ui::PageTransition transition,
                                       bool is_renderer_initiated)
    : OpenURLParams(url,
                    GURL(),
                    referrer,
                    disposition,
                    transition,
                    is_renderer_initiated) {}

WebState::OpenURLParams::OpenURLParams(const OpenURLParams& params) = default;

WebState::OpenURLParams& WebState::OpenURLParams::operator=(
    const OpenURLParams& params) = default;

WebState::OpenURLParams::OpenURLParams(OpenURLParams&& params) = default;

WebState::OpenURLParams& WebState::OpenURLParams::operator=(
    OpenURLParams&& params) = default;

WebState::OpenURLParams::~OpenURLParams() {}

WebState::InterfaceBinder::InterfaceBinder(WebState* web_state)
    : web_state_(web_state) {}

WebState::InterfaceBinder::~InterfaceBinder() = default;

void WebState::InterfaceBinder::AddInterface(std::string_view interface_name,
                                             Callback callback) {
  callbacks_.emplace(std::string(interface_name), std::move(callback));
}

void WebState::InterfaceBinder::RemoveInterface(
    std::string_view interface_name) {
  callbacks_.erase(std::string(interface_name));
}

void WebState::InterfaceBinder::BindInterface(
    mojo::GenericPendingReceiver receiver) {
  DCHECK(receiver.is_valid());
  auto it = callbacks_.find(*receiver.interface_name());
  if (it != callbacks_.end())
    it->second.Run(&receiver);

  GetWebClient()->BindInterfaceReceiverFromMainFrame(web_state_,
                                                     std::move(receiver));
}

WebState::InterfaceBinder* WebState::GetInterfaceBinderForMainFrame() {
  return nullptr;
}

}  // namespace web
