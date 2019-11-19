// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/http_server_properties_factory.h"

#include <memory>

#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/http/http_server_properties.h"

namespace {

class PrefServiceAdapter : public net::HttpServerProperties::PrefDelegate,
                           public PrefStore::Observer {
 public:
  explicit PrefServiceAdapter(scoped_refptr<JsonPrefStore> pref_store)
      : pref_store_(std::move(pref_store)),
        path_(prefs::kHttpServerProperties) {}

  ~PrefServiceAdapter() override {
    if (on_pref_load_callback_)
      pref_store_->RemoveObserver(this);
  }

  // PrefDelegate implementation.
  const base::DictionaryValue* GetServerProperties() const override {
    const base::Value* value;
    if (pref_store_->GetValue(path_, &value)) {
      const base::DictionaryValue* dict;
      if (value->GetAsDictionary(&dict))
        return dict;
    }

    return nullptr;
  }
  void SetServerProperties(const base::DictionaryValue& value,
                           base::OnceClosure callback) override {
    pref_store_->SetValue(path_, value.CreateDeepCopy(),
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    if (callback)
      pref_store_->CommitPendingWrite(std::move(callback));
  }
  void WaitForPrefLoad(base::OnceClosure callback) override {
    DCHECK(!on_pref_load_callback_);
    if (!pref_store_->IsInitializationComplete()) {
      on_pref_load_callback_ = std::move(callback);
      pref_store_->AddObserver(this);
      return;
    }

    // If prefs have already loaded, invoke the pref observer asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }

  // PrefStore::Observer implementation.
  void OnPrefValueChanged(const std::string& key) override {}
  void OnInitializationCompleted(bool succeeded) override {
    if (on_pref_load_callback_) {
      pref_store_->RemoveObserver(this);
      std::move(on_pref_load_callback_).Run();
    }
  }

 private:
  scoped_refptr<JsonPrefStore> pref_store_;
  const std::string path_;

  // Only non-null while waiting for initial pref load. |this| is observes the
  // |pref_store_| exactly when non-null.
  base::OnceClosure on_pref_load_callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceAdapter);
};

}  // namespace

// static
std::unique_ptr<net::HttpServerProperties>
HttpServerPropertiesFactory::CreateHttpServerProperties(
    scoped_refptr<JsonPrefStore> pref_store,
    net::NetLog* net_log) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return std::make_unique<net::HttpServerProperties>(
      std::make_unique<PrefServiceAdapter>(std::move(pref_store)), net_log);
}
