// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_application_mac.h"

#include "base/auto_reset.h"
#include "base/observer_list.h"
#include "content/public/browser/native_event_processor_mac.h"
#include "content/public/browser/native_event_processor_observer_mac.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "url/gurl.h"

@interface ShellCrApplication () <NativeEventProcessor>
@end

@implementation ShellCrApplication {
  base::ObserverList<content::NativeEventProcessorObserver>::Unchecked
      _observers;

  BOOL _handlingSendEvent;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  base::AutoReset<BOOL> scoper(&_handlingSendEvent, YES);

  content::ScopedNotifyNativeEventProcessorObserver scopedObserverNotifier(
      &_observers, event);
  [super sendEvent:event];
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (IBAction)newDocument:(id)sender {
  content::ShellBrowserContext* browserContext =
      content::ShellContentBrowserClient::Get()->browser_context();
  content::Shell::CreateNewWindow(browserContext, GURL(url::kAboutBlankURL),
                                  nullptr, gfx::Size());
}

- (void)addNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  _observers.AddObserver(observer);
}

- (void)removeNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  _observers.RemoveObserver(observer);
}

@end
