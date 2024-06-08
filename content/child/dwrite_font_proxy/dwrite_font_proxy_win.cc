// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/dwrite_font_proxy/dwrite_localized_strings_win.h"
#include "content/public/child/child_thread.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

// Limits to 20 the number of family names that can be accessed by a renderer.
// This feature will be enabled for a subset of users to assess the impact on
// input delay. It will not ship as-is, because it breaks some pages. Local
// experiments show that accessing >20 fonts is typically done by fingerprinting
// scripts.
// TODO(crbug.com/40133493): Remove this feature when the experiment is
// complete. If the experiment shows a significant input delay improvement,
// replace with a more refined mitigation for pages that access many fonts.
BASE_FEATURE(kLimitFontFamilyNamesPerRenderer,
             "LimitFontFamilyNamesPerRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr size_t kFamilyNamesLimit = 20;

// Family names that opted-out from the limit enforced by
// |kLimitFontFamilyNamesPerRenderer|. This is required because Blink uses these
// fonts as last resort and crashes if they can't be loaded.
const char16_t* kLastResortFontNames[] = {
    u"Sans",     u"Arial",   u"MS UI Gothic",    u"Microsoft Sans Serif",
    u"Segoe UI", u"Calibri", u"Times New Roman", u"Courier New"};

bool IsLastResortFontName(const std::u16string& font_name) {
  for (const char16_t* last_resort_font_name : kLastResortFontNames) {
    if (font_name == last_resort_font_name)
      return true;
  }
  return false;
}

// Binds a DWriteFontProxy pending receiver. Must be invoked from the main
// thread.
void BindHostReceiverOnMainThread(
    mojo::PendingReceiver<blink::mojom::DWriteFontProxy> pending_receiver) {
  ChildThread::Get()->BindHostReceiver(std::move(pending_receiver));
}

}  // namespace

HRESULT DWriteFontCollectionProxy::Create(
    DWriteFontCollectionProxy** proxy_out,
    IDWriteFactory* dwrite_factory,
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy) {
  return Microsoft::WRL::MakeAndInitialize<DWriteFontCollectionProxy>(
      proxy_out, dwrite_factory, std::move(proxy));
}

DWriteFontCollectionProxy::DWriteFontCollectionProxy() = default;

DWriteFontCollectionProxy::~DWriteFontCollectionProxy() = default;

// TODO(crbug.com/40200438): Confirm this is useful and remove it otherwise.
void DWriteFontCollectionProxy::InitializePrewarmer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |PrewarmFamilyOnWorker| invokes |GetFontProxy| while holding a
  // |family_lock_|. If |GetFontProxy| is invoked from a sequence for which no
  // |mojo::Remote<blink::mojom::DWriteFontProxy>| exists in sequence-local
  // storage, it posts to the main thread to bind a new one. To avoid a
  // potential deadlock on a |family_lock_| during that operation,
  // pre-initialize the |mojo::Remote<blink::mojom::DWriteFontProxy>| for
  // |prewarm_task_runner_| here.
  //
  // Also, to avoid creating too many
  // |mojo::Remote<blink::mojom::DWriteFontProxy>|, a single SequencedTaskRunner
  // is used for all font prewarming operations.
  DCHECK(!prewarm_task_runner_);
  prewarm_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock()});

  // Let |prewarm_task_runner_| bind its |font_proxy_|. This needs to be done in
  // the sequence, because |font_proxy_| is stored in
  // |SequenceLocalStorageSlot|.
  mojo::PendingRemote<blink::mojom::DWriteFontProxy> font_proxy;
  BindHostReceiverOnMainThread(font_proxy.InitWithNewPipeAndPassReceiver());
  prewarm_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DWriteFontCollectionProxy::BindFontProxy,
          // |DWriteFontCollectionProxy| is kept in global, never destructed.
          base::Unretained(this), std::move(font_proxy)));
}

void DWriteFontCollectionProxy::InitializePrewarmerForTesting(
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> remote) {
  // See |InitializePrewarmer|.
  prewarm_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock()});
  prewarm_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DWriteFontCollectionProxy::BindFontProxy,
          // |DWriteFontCollectionProxy| is kept in global, never destructed.
          base::Unretained(this), std::move(remote)));
}

inline DWriteFontFamilyProxy* DWriteFontCollectionProxy::GetFamilyLockRequired(
    UINT32 family_index) {
  if (family_index < families_.size())
    return families_[family_index].Get();
  return nullptr;
}

DWriteFontFamilyProxy* DWriteFontCollectionProxy::GetFamily(
    UINT32 family_index) {
  base::AutoLock families_lock(families_lock_);
  return GetFamilyLockRequired(family_index);
}

HRESULT DWriteFontCollectionProxy::FindFamilyName(const WCHAR* family_name,
                                                  UINT32* index,
                                                  BOOL* exists) {
  DCHECK(family_name);
  static_assert(sizeof(WCHAR) == sizeof(char16_t), "WCHAR should be UTF-16.");
  const std::u16string name(reinterpret_cast<const char16_t*>(family_name));
  return FindFamilyName(name, index, exists);
}

HRESULT DWriteFontCollectionProxy::FindFamilyName(
    const std::u16string& family_name,
    UINT32* index,
    BOOL* exists) {
  DCHECK(index);
  DCHECK(exists);
  TRACE_EVENT0("dwrite,fonts", "FontProxy::FindFamilyName");

  HRESULT hr = S_OK;
  if (std::optional<UINT32> family_index = FindFamilyIndex(family_name, &hr)) {
    DCHECK_EQ(hr, S_OK);
    DCHECK(IsValidFamilyIndex(*family_index));
    *index = *family_index;
    *exists = TRUE;
  } else {
    // |hr| can be failures, or |S_OK| if the |family_name| is not found.
    *exists = FALSE;
    *index = kFamilyNotFound;
  }
  return hr;
}

std::optional<UINT32> DWriteFontCollectionProxy::FindFamilyIndex(
    const std::u16string& family_name,
    HRESULT* hresult_out) {
  DCHECK(!hresult_out || *hresult_out == S_OK);
  {
    base::AutoLock families_lock(families_lock_);
    auto iter = family_names_.find(family_name);
    if (iter != family_names_.end()) {
      if (iter->second != kFamilyNotFound)
        return iter->second;
      return std::nullopt;
    }

    if (base::FeatureList::IsEnabled(kLimitFontFamilyNamesPerRenderer) &&
        family_names_.size() > kFamilyNamesLimit &&
        !IsLastResortFontName(family_name)) {
      return std::nullopt;
    }
  }

  // Release the lock while making the |FindFamily| sync mojo call. Crash logs
  // indicate that this may hang, or take long, for offscreen canvas. Releasing
  // the lock protects the main thread in such case. crbug.com/1289576
  uint32_t family_index = 0;
  if (!GetFontProxy().FindFamily(family_name, &family_index)) {
    if (hresult_out)
      *hresult_out = E_FAIL;
    return std::nullopt;
  }

  {
    base::AutoLock families_lock(families_lock_);
    DCHECK(family_names_.find(family_name) == family_names_.end() ||
           family_names_[family_name] == family_index);
    family_names_[family_name] = family_index;
    if (family_index == kFamilyNotFound) [[unlikely]] {
      return std::nullopt;
    }
    DCHECK(IsValidFamilyIndex(family_index));

    if (DWriteFontFamilyProxy* family =
            GetOrCreateFamilyLockRequired(family_index)) {
      family->SetName(family_name);
      return family_index;
    }

    if (hresult_out)
      *hresult_out = E_FAIL;
    return std::nullopt;
  }
}

DWriteFontFamilyProxy* DWriteFontCollectionProxy::FindFamily(
    const std::u16string& family_name) {
  if (const std::optional<UINT32> family_index = FindFamilyIndex(family_name)) {
    if (DWriteFontFamilyProxy* family = GetFamily(*family_index))
      return family;
  }
  return nullptr;
}

void DWriteFontCollectionProxy::PrewarmFamily(
    const blink::WebString& family_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prewarm_task_runner_) {
    // |BindHostReceiverOnMainThread| requires |ChildThread::Get()|, but it may
    // not be available in some tests. Disable the prewarmer.
    if (!ChildThread::Get()) [[unlikely]] {
      return;
    }
    InitializePrewarmer();
  }

  DCHECK(prewarm_task_runner_);
  prewarm_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DWriteFontCollectionProxy::PrewarmFamilyOnWorker,
                     // |this| is kept in global, never destructed.
                     base::Unretained(this), family_name.Utf16()));
}

void DWriteFontCollectionProxy::PrewarmFamilyOnWorker(
    const std::u16string family_name) {
  base::ScopedAllowBaseSyncPrimitives allow_sync;
  if (DWriteFontFamilyProxy* family = FindFamily(family_name))
    family->PrewarmFamilyOnWorker();
}

HRESULT DWriteFontCollectionProxy::GetFontFamily(
    UINT32 index,
    IDWriteFontFamily** font_family) {
  DCHECK(font_family);
  base::AutoLock families_lock(families_lock_);

  if (index < families_.size() && families_[index]) {
    families_[index].CopyTo(font_family);
    return S_OK;
  }

  if (DWriteFontFamilyProxy* family = GetOrCreateFamilyLockRequired(index)) {
    const mswr::ComPtr<DWriteFontFamilyProxy> family_ptr = family;
    family_ptr.CopyTo(font_family);
    return S_OK;
  }
  return E_FAIL;
}

UINT32 DWriteFontCollectionProxy::GetFontFamilyCount() {
  base::AutoLock families_lock(families_lock_);
  return GetFontFamilyCountLockRequired();
}

UINT32 DWriteFontCollectionProxy::GetFontFamilyCountLockRequired() {
  families_lock_.AssertAcquired();

  if (family_count_ != UINT_MAX)
    return family_count_;

  TRACE_EVENT0("dwrite,fonts", "FontProxy::GetFontFamilyCount");

  uint32_t family_count = 0;
  if (!GetFontProxy().GetFamilyCount(&family_count)) {
    return 0;
  }

  family_count_ = family_count;
  return family_count;
}

HRESULT DWriteFontCollectionProxy::GetFontFromFontFace(
    IDWriteFontFace* font_face,
    IDWriteFont** font) {
  DCHECK(font_face);
  DCHECK(font);
  base::AutoLock families_lock(families_lock_);

  for (const auto& family : families_) {
    if (family && family->GetFontFromFontFace(font_face, font)) {
      return S_OK;
    }
  }
  // If the font came from our collection, at least one family should match
  DCHECK(false);

  return E_FAIL;
}

// Code downstream from DWrite's |CreateCustomFontCollection| calls this
// function back.
//
// |families_[family_index]| should be locked, but |families_| may or may not be
// locked. This function is so expensive that reading, writing, or locking
// |families_| should be avoided.
HRESULT DWriteFontCollectionProxy::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    const void* collection_key,
    UINT32 collection_key_size,
    IDWriteFontFileEnumerator** font_file_enumerator) {
  if (!collection_key || collection_key_size != sizeof(uint32_t)) {
    return E_INVALIDARG;
  }

  TRACE_EVENT0("dwrite,fonts", "FontProxy::LoadingFontFiles");

  const uint32_t* family_index =
      reinterpret_cast<const uint32_t*>(collection_key);

  std::vector<base::File> file_handles;
  {
    // The Mojo call cannot be asynchronous because CreateEnumeratorFromKey is
    // invoked synchronously by DWrite. |ScopedAllowBaseSyncPrimitives| is
    // needed to allow the synchronous Mojo call from the ThreadPool
    // (CreateEnumeratorFromKey can be invoked from the main thread or the
    // ThreadPool).
    base::ScopedAllowBaseSyncPrimitives allow_sync;
    if (!GetFontProxy().GetFontFileHandles(*family_index, &file_handles)) {
      return E_FAIL;
    }
  }

  std::vector<HANDLE> handles;
  handles.reserve(file_handles.size());
  for (auto& file_handle : file_handles) {
    handles.push_back(file_handle.TakePlatformFile());
  }

  // This leaks the handles, since they are used as the reference key to
  // CreateStreamFromKey, and DirectWrite requires the reference keys to
  // remain valid for the lifetime of the loader. The loader is the font
  // collection proxy, which remains alive for the lifetime of the renderer.
  HRESULT hr = mswr::MakeAndInitialize<FontFileEnumerator>(
      font_file_enumerator, factory, this, &handles);

  if (!SUCCEEDED(hr)) {
    DCHECK(false);
    return E_FAIL;
  }

  return S_OK;
}

HRESULT DWriteFontCollectionProxy::CreateStreamFromKey(
    const void* font_file_reference_key,
    UINT32 font_file_reference_key_size,
    IDWriteFontFileStream** font_file_stream) {
  if (font_file_reference_key_size != sizeof(HANDLE)) {
    return E_FAIL;
  }

  TRACE_EVENT0("dwrite,fonts", "FontFileEnumerator::CreateStreamFromKey");

  HANDLE file_handle =
      *reinterpret_cast<const HANDLE*>(font_file_reference_key);

  if (file_handle == NULL || file_handle == INVALID_HANDLE_VALUE) {
    DCHECK(false);
    return E_FAIL;
  }

  mswr::ComPtr<FontFileStream> stream;
  if (!SUCCEEDED(
          mswr::MakeAndInitialize<FontFileStream>(&stream, file_handle))) {
    return E_FAIL;
  }
  *font_file_stream = stream.Detach();
  return S_OK;
}

HRESULT DWriteFontCollectionProxy::RuntimeClassInitialize(
    IDWriteFactory* factory,
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy) {
  DCHECK(factory);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  factory_ = factory;
  if (proxy)
    font_proxy_.GetOrCreateValue().Bind(std::move(proxy));
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  // |prewarm_task_runner_| needs to be initialized later because ThreadPool is
  // not setup yet when |this| is instantiated. See |InitializePrewarmer|.

  HRESULT hr = factory->RegisterFontCollectionLoader(this);
  DCHECK(SUCCEEDED(hr));
  hr = factory_->RegisterFontFileLoader(this);
  DCHECK(SUCCEEDED(hr));
  return S_OK;
}

void DWriteFontCollectionProxy::Unregister() {
  factory_->UnregisterFontCollectionLoader(this);
  factory_->UnregisterFontFileLoader(this);
}

// |families_[family_index]| should be locked, but |families_| may or may not be
// locked. This function is so expensive that reading, writing, or locking
// |families_| should be avoided.
bool DWriteFontCollectionProxy::LoadFamily(
    UINT32 family_index,
    IDWriteFontCollection** containing_collection) {
  TRACE_EVENT0("dwrite,fonts", "FontProxy::LoadFamily");

  uint32_t index = family_index;
  // CreateCustomFontCollection ends up calling
  // DWriteFontCollectionProxy::CreateEnumeratorFromKey.
  HRESULT hr = factory_->CreateCustomFontCollection(
      this /*collectionLoader*/, reinterpret_cast<const void*>(&index),
      sizeof(index), containing_collection);

  return SUCCEEDED(hr);
}

bool DWriteFontCollectionProxy::GetFontFamily(UINT32 family_index,
                                              const std::u16string& family_name,
                                              IDWriteFontFamily** font_family) {
  DCHECK(font_family);
  DCHECK(!family_name.empty());
  base::AutoLock families_lock(families_lock_);

  DWriteFontFamilyProxy* family = GetOrCreateFamilyLockRequired(family_index);
  if (!family)
    return false;

  family->SetNameIfNotLoaded(family_name);

  const mswr::ComPtr<DWriteFontFamilyProxy> family_ptr = family;
  family_ptr.CopyTo(font_family);
  return true;
}

bool DWriteFontCollectionProxy::LoadFamilyNames(
    UINT32 family_index,
    IDWriteLocalizedStrings** localized_strings) {
  TRACE_EVENT0("dwrite,fonts", "FontProxy::LoadFamilyNames");

  std::vector<blink::mojom::DWriteStringPairPtr> pairs;
  if (!GetFontProxy().GetFamilyNames(family_index, &pairs)) {
    return false;
  }
  std::vector<std::pair<std::u16string, std::u16string>> strings;
  for (auto& pair : pairs) {
    strings.emplace_back(pair->first, pair->second);
  }

  HRESULT hr = mswr::MakeAndInitialize<DWriteLocalizedStrings>(
      localized_strings, &strings);

  return SUCCEEDED(hr);
}

DWriteFontFamilyProxy* DWriteFontCollectionProxy::GetOrCreateFamilyLockRequired(
    UINT32 family_index) {
  DCHECK(IsValidFamilyIndex(family_index));

  if (family_index < families_.size()) {
    if (DWriteFontFamilyProxy* family = families_[family_index].Get())
      return family;
  }

  const UINT32 family_count = GetFontFamilyCountLockRequired();
  if (family_index >= family_count)
    return nullptr;

  if (families_.size() < family_count)
    families_.resize(family_count);

  mswr::ComPtr<DWriteFontFamilyProxy> family;
  HRESULT hr = mswr::MakeAndInitialize<DWriteFontFamilyProxy>(&family, this,
                                                              family_index);
  DCHECK(SUCCEEDED(hr));
  DCHECK_LT(family_index, families_.size());

  families_[family_index] = family;
  return family.Get();
}

blink::mojom::DWriteFontProxy& DWriteFontCollectionProxy::GetFontProxy() {
  mojo::Remote<blink::mojom::DWriteFontProxy>& font_proxy =
      font_proxy_.GetOrCreateValue();
  if (!font_proxy) {
    if (main_task_runner_->RunsTasksInCurrentSequence()) {
      BindHostReceiverOnMainThread(font_proxy.BindNewPipeAndPassReceiver());
    } else {
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&BindHostReceiverOnMainThread,
                                    font_proxy.BindNewPipeAndPassReceiver()));
    }
  }
  return *font_proxy;
}

void DWriteFontCollectionProxy::BindFontProxyUsingBroker(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* interface_broker) {
  mojo::Remote<blink::mojom::DWriteFontProxy>& font_proxy =
      font_proxy_.GetOrCreateValue();
  DCHECK(!font_proxy);
  interface_broker->GetInterface(font_proxy.BindNewPipeAndPassReceiver());
}

void DWriteFontCollectionProxy::BindFontProxy(
    mojo::PendingRemote<blink::mojom::DWriteFontProxy> remote) {
  mojo::Remote<blink::mojom::DWriteFontProxy>& font_proxy =
      font_proxy_.GetOrCreateValue();
  DCHECK(!font_proxy);
  font_proxy.Bind(std::move(remote));
}

DWriteFontFamilyProxy::DWriteFontFamilyProxy() = default;

DWriteFontFamilyProxy::~DWriteFontFamilyProxy() = default;

HRESULT DWriteFontFamilyProxy::GetFontCollection(
    IDWriteFontCollection** font_collection) {
  DCHECK(font_collection);

  proxy_collection_.CopyTo(font_collection);
  return S_OK;
}

UINT32 DWriteFontFamilyProxy::GetFontCount() {
  // We could conceivably proxy just the font count. However, calling
  // GetFontCount is almost certain to be followed by a series of GetFont
  // calls which will need to load all the fonts anyway, so we might as
  // well save an IPC here.
  if (IDWriteFontFamily* family = LoadFamily())
    return family->GetFontCount();
  return 0;
}

HRESULT DWriteFontFamilyProxy::GetFont(UINT32 index, IDWriteFont** font) {
  DCHECK(font);

  if (index >= GetFontCount())
    return E_INVALIDARG;
  if (IDWriteFontFamily* family = LoadFamily())
    return family->GetFont(index, font);
  return E_FAIL;
}

HRESULT DWriteFontFamilyProxy::GetFamilyNames(IDWriteLocalizedStrings** names) {
  DCHECK(names);
  base::AutoLock family_lock(family_lock_);

  // Prefer the real thing, if available.
  if (family_) {
    family_names_.Reset();  // Release cached data.
    return family_->GetFamilyNames(names);
  }

  // If already cached, use the cache.
  if (family_names_) {
    family_names_.CopyTo(names);
    return S_OK;
  }

  TRACE_EVENT0("dwrite,fonts", "FontProxy::GetFamilyNames");

  // Otherwise, do the IPC.
  if (!proxy_collection_->LoadFamilyNames(family_index_, &family_names_))
    return E_FAIL;

  family_names_.CopyTo(names);
  return S_OK;
}

HRESULT DWriteFontFamilyProxy::GetFirstMatchingFont(
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style,
    IDWriteFont** matching_font) {
  DCHECK(matching_font);

  if (IDWriteFontFamily* family = LoadFamily())
    return family->GetFirstMatchingFont(weight, stretch, style, matching_font);
  return E_FAIL;
}

HRESULT DWriteFontFamilyProxy::GetMatchingFonts(
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style,
    IDWriteFontList** matching_fonts) {
  DCHECK(matching_fonts);

  if (IDWriteFontFamily* family = LoadFamily())
    return family->GetMatchingFonts(weight, stretch, style, matching_fonts);
  return E_FAIL;
}

HRESULT DWriteFontFamilyProxy::RuntimeClassInitialize(
    DWriteFontCollectionProxy* collection,
    UINT32 index) {
  DCHECK(collection);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proxy_collection_ = collection;
  family_index_ = index;
  return S_OK;
}

bool DWriteFontFamilyProxy::GetFontFromFontFace(IDWriteFontFace* font_face,
                                                IDWriteFont** font) {
  DCHECK(font_face);
  DCHECK(font);
  base::AutoLock family_lock(family_lock_);

  if (!family_)
    return false;

  mswr::ComPtr<IDWriteFontCollection> collection;
  HRESULT hr = family_->GetFontCollection(&collection);
  DCHECK(SUCCEEDED(hr));
  hr = collection->GetFontFromFontFace(font_face, font);

  return SUCCEEDED(hr);
}

void DWriteFontFamilyProxy::SetName(const std::u16string& family_name) {
  base::AutoLock family_lock(family_lock_);
  family_name_.assign(family_name);
}

void DWriteFontFamilyProxy::SetNameIfNotLoaded(
    const std::u16string& family_name) {
  base::AutoLock family_lock(family_lock_);
  if (!family_ || family_name_.empty())
    family_name_.assign(family_name);
}

const std::u16string& DWriteFontFamilyProxy::GetName() {
  base::AutoLock family_lock(family_lock_);
  return family_name_;
}

IDWriteFontFamily* DWriteFontFamilyProxy::LoadFamily() {
  TRACE_EVENT0("dwrite,fonts", "DWriteFontFamilyProxy::LoadFamily");

  base::AutoLock family_lock(family_lock_);

  if (family_)
    return family_.Get();
  return LoadFamilyCoreLockRequired();
}

void DWriteFontFamilyProxy::PrewarmFamilyOnWorker() {
  // Load the family only if other threads haven't loaded this family.
  base::AutoLock family_lock(family_lock_);
  if (!family_)
    LoadFamilyCoreLockRequired();
}

// Note this function may run in the main thread, or in a worker thread.
IDWriteFontFamily* DWriteFontFamilyProxy::LoadFamilyCoreLockRequired() {
  DCHECK(!family_);
  family_lock_.AssertAcquired();

  // TODO(dcheng): Is this crash key still used? There does not appear to be
  // anything obvious below that would trigger a crash report.
  SCOPED_CRASH_KEY_STRING32("LoadFamily", "font_key_name",
                            base::UTF16ToUTF8(family_name_));

  mswr::ComPtr<IDWriteFontCollection> collection;
  if (!proxy_collection_->LoadFamily(family_index_, &collection)) {
    return nullptr;
  }

  UINT32 family_count = collection->GetFontFamilyCount();

  HRESULT hr;
  if (family_count > 1) {
    // Some fonts are packaged in a single file containing multiple families. In
    // such a case we can find the right family by family name.
    DCHECK(!family_name_.empty());
    UINT32 family_index = 0;
    BOOL found = FALSE;
    static_assert(sizeof(WCHAR) == sizeof(char16_t), "WCHAR should be UTF-16.");
    hr = collection->FindFamilyName(
        reinterpret_cast<const WCHAR*>(family_name_.c_str()), &family_index,
        &found);
    if (SUCCEEDED(hr) && found) {
      hr = collection->GetFontFamily(family_index, &family_);
      return SUCCEEDED(hr) ? family_.Get() : nullptr;
    }
  }

  DCHECK_LE(family_count, 1u);

  if (family_count == 0) {
    // This is really strange, we successfully loaded no fonts?!
    return nullptr;
  }

  hr = collection->GetFontFamily(0, &family_);
  return SUCCEEDED(hr) ? family_.Get() : nullptr;
}

FontFileEnumerator::FontFileEnumerator() = default;

FontFileEnumerator::~FontFileEnumerator() = default;

HRESULT FontFileEnumerator::GetCurrentFontFile(IDWriteFontFile** file) {
  DCHECK(file);
  if (current_file_ >= files_.size()) {
    return E_FAIL;
  }

  TRACE_EVENT0("dwrite,fonts", "FontFileEnumerator::GetCurrentFontFile");

  // CreateCustomFontFileReference ends up calling
  // DWriteFontCollectionProxy::CreateStreamFromKey.
  HRESULT hr = factory_->CreateCustomFontFileReference(
      reinterpret_cast<const void*>(&files_[current_file_]),
      sizeof(files_[current_file_]), loader_.Get() /*IDWriteFontFileLoader*/,
      file);
  DCHECK(SUCCEEDED(hr));
  return hr;
}

HRESULT FontFileEnumerator::MoveNext(BOOL* has_current_file) {
  DCHECK(has_current_file);

  TRACE_EVENT0("dwrite,fonts", "FontFileEnumerator::MoveNext");
  if (next_file_ >= files_.size()) {
    *has_current_file = FALSE;
    current_file_ = UINT_MAX;
    return S_OK;
  }

  current_file_ = next_file_;
  ++next_file_;
  *has_current_file = TRUE;
  return S_OK;
}

HRESULT FontFileEnumerator::RuntimeClassInitialize(
    IDWriteFactory* factory,
    IDWriteFontFileLoader* loader,
    std::vector<HANDLE>* files) {
  factory_ = factory;
  loader_ = loader;
  files_.swap(*files);
  return S_OK;
}

FontFileStream::FontFileStream() = default;

FontFileStream::~FontFileStream() = default;

HRESULT FontFileStream::GetFileSize(UINT64* file_size) {
  *file_size = data_.length();
  return S_OK;
}

HRESULT FontFileStream::GetLastWriteTime(UINT64* last_write_time) {
  *last_write_time = 0;
  return S_OK;
}

HRESULT FontFileStream::ReadFileFragment(const void** fragment_start,
                                         UINT64 fragment_offset,
                                         UINT64 fragment_size,
                                         void** fragment_context) {
  if (fragment_offset + fragment_size < fragment_offset)
    return E_FAIL;
  if (fragment_offset + fragment_size > data_.length())
    return E_FAIL;
  *fragment_start = data_.data() + fragment_offset;
  *fragment_context = nullptr;
  return S_OK;
}

HRESULT FontFileStream::RuntimeClassInitialize(HANDLE handle) {
  // Duplicate the original handle so we can reopen the file after the memory
  // mapped section closes it.
  HANDLE duplicate_handle;
  if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                       &duplicate_handle, 0 /* dwDesiredAccess */,
                       false /* bInheritHandle */, DUPLICATE_SAME_ACCESS)) {
    return E_FAIL;
  }

  if (!data_.Initialize(base::File(duplicate_handle))) {
    return E_FAIL;
  }
  return S_OK;
}

}  // namespace content
