// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_FACTORY_H_
#define EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class LazyBackgroundTaskQueue;

class LazyBackgroundTaskQueueFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  LazyBackgroundTaskQueueFactory(const LazyBackgroundTaskQueueFactory&) =
      delete;
  LazyBackgroundTaskQueueFactory& operator=(
      const LazyBackgroundTaskQueueFactory&) = delete;

  static LazyBackgroundTaskQueue* GetForBrowserContext(
      content::BrowserContext* context);
  static LazyBackgroundTaskQueueFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LazyBackgroundTaskQueueFactory>;

  LazyBackgroundTaskQueueFactory();
  ~LazyBackgroundTaskQueueFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAZY_BACKGROUND_TASK_QUEUE_FACTORY_H_
