// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <string_view>

#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state_observer.h"

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

WebState::ScopedWebContentCoverer::ScopedWebContentCoverer(
    WebState* web_state) {
  if (web_state) {
    web_state_ = web_state->GetWeakPtr();
    web_state->DidCoverWebContent();
  }
}

WebState::ScopedWebContentCoverer::~ScopedWebContentCoverer() {
  if (web_state_) {
    web_state_->DidRevealWebContent();
  }
}

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
  if (it != callbacks_.end()) {
    it->second.Run(&receiver);
  }

  GetWebClient()->BindInterfaceReceiverFromMainFrame(web_state_,
                                                     std::move(receiver));
}

WebState::InterfaceBinder* WebState::GetInterfaceBinderForMainFrame() {
  return nullptr;
}

WebState* WebState::ForceRealized() {
  return ForceRealizedWithPolicy(RealizationPolicy::kDefault);
}

void WebState::NotifyWebStateRealized(WebStateObserverList& observers) {
  DCHECK(IsRealized());

  // base::ObserverList does not provide an easy way to iterate only on
  // the currently registered observers on a per-call basis (the policy
  // is only for the whole list).
  //
  // Emulate this by creating an iterator that is considered at the end
  // of the base::ObserverList by successive increment. This relies on
  // implementation details of base::ObserverList (the fact that while
  // there is any living iterator, the list won't be compacted even if
  // an observer is removed, and that all registered observers will be
  // added at the end of the list).
  auto end = observers.begin();
  while (end != observers.end()) {
    ++end;
  }

  // Iterate again, this time calling WebStateRealized(this) on all the
  // pre-existing iterator. If any new observers are registered they will
  // be added after `end` and the iteration won't reach them.
  for (auto iter = observers.begin(); iter != end; ++iter) {
    iter->WebStateRealized(this);
  }
}

}  // namespace web
