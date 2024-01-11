// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_WIN_H_
#define CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_WIN_H_

#include <dwrite.h>
#include <wrl.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom.h"
#include "third_party/blink/public/platform/web_font_rendering_client.h"

namespace content {

class DWriteFontFamilyProxy;

// Implements a DirectWrite font collection that uses IPC to the browser to do
// font enumeration. If a matching family is found, it will be loaded locally
// into a custom font collection.
// This is needed because the sandbox interferes with DirectWrite's
// communication with the system font service.
// This class can be accessed from any thread, uses locks and thread annotations
// to coordinate concurrent accesses.
class DWriteFontCollectionProxy
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontCollection,
          IDWriteFontCollectionLoader,
          IDWriteFontFileLoader>,
      public blink::WebFontRenderingClient {
 public:
  // Factory method to avoid exporting the class and all it derives from.
  //
  // |proxy| is an optional DWriteFontProxy to use when the constructed
  // DWriteFontCollectionProxy is used on the current sequence. DWriteFontProxy
  // remotes will be bound via ChildThread when the constructed
  // DWriteFontCollectionProxy is used from a sequence for which no
  // DWriteFontProxy has been provided.
  static CONTENT_EXPORT HRESULT
  Create(DWriteFontCollectionProxy** proxy_out,
         IDWriteFactory* dwrite_factory,
         mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy);

  // Use Create() to construct these objects. Direct calls to the constructor
  // are an error - it is only public because a WRL helper function creates the
  // objects.
  DWriteFontCollectionProxy();

  DWriteFontCollectionProxy& operator=(const DWriteFontCollectionProxy&) =
      delete;

  ~DWriteFontCollectionProxy() override;

  // IDWriteFontCollection:
  HRESULT STDMETHODCALLTYPE FindFamilyName(const WCHAR* family_name,
                                           UINT32* index,
                                           BOOL* exists) override;
  HRESULT STDMETHODCALLTYPE
  GetFontFamily(UINT32 index, IDWriteFontFamily** font_family) override
      LOCKS_EXCLUDED(families_lock_);
  UINT32 STDMETHODCALLTYPE GetFontFamilyCount() override
      LOCKS_EXCLUDED(families_lock_);
  HRESULT STDMETHODCALLTYPE GetFontFromFontFace(IDWriteFontFace* font_face,
                                                IDWriteFont** font) override
      LOCKS_EXCLUDED(families_lock_);

  // IDWriteFontCollectionLoader:
  HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(
      IDWriteFactory* factory,
      const void* collection_key,
      UINT32 collection_key_size,
      IDWriteFontFileEnumerator** font_file_enumerator) override;

  // IDWriteFontFileLoader:
  HRESULT STDMETHODCALLTYPE
  CreateStreamFromKey(const void* font_file_reference_key,
                      UINT32 font_file_reference_key_size,
                      IDWriteFontFileStream** font_file_stream) override;

  CONTENT_EXPORT HRESULT STDMETHODCALLTYPE RuntimeClassInitialize(
      IDWriteFactory* factory,
      mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy);

  CONTENT_EXPORT void Unregister();

  bool LoadFamily(UINT32 family_index,
                  IDWriteFontCollection** containing_collection);

  // Gets the family at the specified index with the expected name. This can be
  // used to avoid an IPC call when both the index and family name are known.
  bool GetFontFamily(UINT32 family_index,
                     const std::u16string& family_name,
                     IDWriteFontFamily** font_family)
      LOCKS_EXCLUDED(families_lock_);

  bool LoadFamilyNames(UINT32 family_index, IDWriteLocalizedStrings** strings);

  // `blink::WebFontRenderingClient` overrides
  void BindFontProxyUsingBroker(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* interface_broker) override;

  void PrewarmFamily(const blink::WebString& family_name) override;

  blink::mojom::DWriteFontProxy& GetFontProxy();

  CONTENT_EXPORT void InitializePrewarmerForTesting(
      mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy);

 private:
  void InitializePrewarmer();
  void BindFontProxy(mojo::PendingRemote<blink::mojom::DWriteFontProxy> remote);
  UINT32 GetFontFamilyCountLockRequired()
      EXCLUSIVE_LOCKS_REQUIRED(families_lock_);
  DWriteFontFamilyProxy* GetFamily(UINT32 family_index)
      LOCKS_EXCLUDED(families_lock_);
  DWriteFontFamilyProxy* GetFamilyLockRequired(UINT32 family_index)
      EXCLUSIVE_LOCKS_REQUIRED(families_lock_);
  DWriteFontFamilyProxy* GetOrCreateFamilyLockRequired(UINT32 family_index)
      EXCLUSIVE_LOCKS_REQUIRED(families_lock_);
  std::optional<UINT32> FindFamilyIndex(const std::u16string& family_name,
                                        HRESULT* hresult_out = nullptr)
      LOCKS_EXCLUDED(families_lock_);

  HRESULT FindFamilyName(const std::u16string& family_name,
                         UINT32* index,
                         BOOL* exists);
  DWriteFontFamilyProxy* FindFamily(const std::u16string& family_name);

  void PrewarmFamilyOnWorker(const std::u16string family_name);

  // Special values for |family_names_|.
  enum FamilyIndex : UINT32 { kFamilyNotFound = UINT32_MAX };
  static bool IsValidFamilyIndex(UINT32 index) {
    return index != kFamilyNotFound;
  }

  base::Lock families_lock_;

  // This is initialized in ctor (RuntimeClassInitialize) and will not change.
  Microsoft::WRL::ComPtr<IDWriteFactory> factory_;

  std::vector<Microsoft::WRL::ComPtr<DWriteFontFamilyProxy>> families_
      GUARDED_BY(families_lock_);
  std::map<std::u16string, UINT32> family_names_ GUARDED_BY(families_lock_);
  UINT32 family_count_ GUARDED_BY(families_lock_) = UINT_MAX;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> prewarm_task_runner_;
  // Per-sequence mojo::Remote<DWriteFontProxy>. This is preferred to a
  // mojo::SharedRemote, which would force a thread hop for each call that
  // doesn't originate from the "bound" sequence.
  base::SequenceLocalStorageSlot<mojo::Remote<blink::mojom::DWriteFontProxy>>
      font_proxy_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Implements the DirectWrite font family interface. This class is just a
// stub, until something calls a method that requires actual font data. At that
// point this will load the font files into a custom collection and
// subsequently calls will be proxied to the resulting DirectWrite object.
class DWriteFontFamilyProxy
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontFamily> {
 public:
  DWriteFontFamilyProxy();

  DWriteFontFamilyProxy& operator=(const DWriteFontFamilyProxy&) = delete;

  ~DWriteFontFamilyProxy() override;

  // IDWriteFontFamily:
  HRESULT STDMETHODCALLTYPE
  GetFontCollection(IDWriteFontCollection** font_collection) override;
  UINT32 STDMETHODCALLTYPE GetFontCount() override;
  HRESULT STDMETHODCALLTYPE GetFont(UINT32 index, IDWriteFont** font) override;
  HRESULT STDMETHODCALLTYPE GetFamilyNames(
      IDWriteLocalizedStrings** names) override LOCKS_EXCLUDED(family_lock_);
  HRESULT STDMETHODCALLTYPE
  GetFirstMatchingFont(DWRITE_FONT_WEIGHT weight,
                       DWRITE_FONT_STRETCH stretch,
                       DWRITE_FONT_STYLE style,
                       IDWriteFont** matching_font) override;
  HRESULT STDMETHODCALLTYPE
  GetMatchingFonts(DWRITE_FONT_WEIGHT weight,
                   DWRITE_FONT_STRETCH stretch,
                   DWRITE_FONT_STYLE style,
                   IDWriteFontList** matching_fonts) override;

  HRESULT STDMETHODCALLTYPE
  RuntimeClassInitialize(DWriteFontCollectionProxy* collection, UINT32 index);

  bool GetFontFromFontFace(IDWriteFontFace* font_face, IDWriteFont** font)
      LOCKS_EXCLUDED(family_lock_);

  const std::u16string& GetName() LOCKS_EXCLUDED(family_lock_);
  void SetName(const std::u16string& family_name) LOCKS_EXCLUDED(family_lock_);
  void SetNameIfNotLoaded(const std::u16string& family_name)
      LOCKS_EXCLUDED(family_lock_);

  void PrewarmFamilyOnWorker() LOCKS_EXCLUDED(family_lock_);

 private:
  IDWriteFontFamily* LoadFamily() LOCKS_EXCLUDED(family_lock_);
  IDWriteFontFamily* LoadFamilyCoreLockRequired()
      EXCLUSIVE_LOCKS_REQUIRED(family_lock_);

  base::Lock family_lock_;

  // These are initialized in ctor (RuntimeClassInitialize) and will not change.
  UINT32 family_index_;
  Microsoft::WRL::ComPtr<DWriteFontCollectionProxy> proxy_collection_;

  Microsoft::WRL::ComPtr<IDWriteFontFamily> family_ GUARDED_BY(family_lock_);
  std::u16string family_name_ GUARDED_BY(family_lock_);
  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names_
      GUARDED_BY(family_lock_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// Implements the DirectWrite font file enumerator interface, backed by a list
// of font files.
class FontFileEnumerator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontFileEnumerator> {
 public:
  FontFileEnumerator();

  FontFileEnumerator& operator=(const FontFileEnumerator&) = delete;

  ~FontFileEnumerator() override;

  // IDWriteFontFileEnumerator:
  HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile** file) override;
  HRESULT STDMETHODCALLTYPE MoveNext(BOOL* has_current_file) override;

  HRESULT STDMETHODCALLTYPE
  RuntimeClassInitialize(IDWriteFactory* factory,
                         IDWriteFontFileLoader* loader,
                         std::vector<HANDLE>* files);

 private:
  Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
  Microsoft::WRL::ComPtr<IDWriteFontFileLoader> loader_;
  std::vector<HANDLE> files_;
  UINT32 next_file_ = 0;
  UINT32 current_file_ = UINT_MAX;
};

// Implements the DirectWrite font file stream interface that maps the file to
// be loaded as a memory mapped file, and subsequently returns pointers into
// the mapped memory block.
class FontFileStream
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontFileStream> {
 public:
  FontFileStream();

  FontFileStream& operator=(const FontFileStream&) = delete;

  ~FontFileStream() override;

  // IDWriteFontFileStream:
  HRESULT STDMETHODCALLTYPE GetFileSize(UINT64* file_size) override;
  HRESULT STDMETHODCALLTYPE GetLastWriteTime(UINT64* last_write_time) override;
  HRESULT STDMETHODCALLTYPE ReadFileFragment(const void** fragment_start,
                                             UINT64 file_offset,
                                             UINT64 fragment_size,
                                             void** fragment_context) override;
  void STDMETHODCALLTYPE ReleaseFileFragment(void* fragment_context) override {}

  HRESULT STDMETHODCALLTYPE RuntimeClassInitialize(HANDLE handle);

 private:
  base::MemoryMappedFile data_;
};

}  // namespace content
#endif  // CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_WIN_H_
