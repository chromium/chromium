// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_WIN_H_
#define CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_WIN_H_

#include <dwrite.h>
#include <wrl.h>

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom.h"

namespace content {

class DWriteFontFamilyProxy;

// Implements a DirectWrite font collection that uses IPC to the browser to do
// font enumeration. If a matching family is found, it will be loaded locally
// into a custom font collection.
// This is needed because the sandbox interferes with DirectWrite's
// communication with the system font service.
class DWriteFontCollectionProxy
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontCollection,
          IDWriteFontCollectionLoader,
          IDWriteFontFileLoader> {
 public:
  // Factory method to avoid exporting the class and all it derives from.
  static CONTENT_EXPORT HRESULT
  Create(DWriteFontCollectionProxy** proxy_out,
         IDWriteFactory* dwrite_factory,
         mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy);

  // Use Create() to construct these objects. Direct calls to the constructor
  // are an error - it is only public because a WRL helper function creates the
  // objects.
  DWriteFontCollectionProxy();
  ~DWriteFontCollectionProxy() override;

  // IDWriteFontCollection:
  HRESULT STDMETHODCALLTYPE FindFamilyName(const WCHAR* family_name,
                                           UINT32* index,
                                           BOOL* exists) override;
  HRESULT STDMETHODCALLTYPE
  GetFontFamily(UINT32 index, IDWriteFontFamily** font_family) override;
  UINT32 STDMETHODCALLTYPE GetFontFamilyCount() override;
  HRESULT STDMETHODCALLTYPE GetFontFromFontFace(IDWriteFontFace* font_face,
                                                IDWriteFont** font) override;

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
                     const base::string16& family_name,
                     IDWriteFontFamily** font_family);

  bool LoadFamilyNames(UINT32 family_index, IDWriteLocalizedStrings** strings);

  bool CreateFamily(UINT32 family_index);

  blink::mojom::DWriteFontProxy& GetFontProxy();

 private:
  void SetProxy(mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy);

  Microsoft::WRL::ComPtr<IDWriteFactory> factory_;
  std::vector<Microsoft::WRL::ComPtr<DWriteFontFamilyProxy>> families_;
  std::map<base::string16, UINT32> family_names_;
  UINT32 family_count_ = UINT_MAX;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<blink::mojom::ThreadSafeDWriteFontProxyPtr> font_proxy_;

  DISALLOW_ASSIGN(DWriteFontCollectionProxy);
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
  ~DWriteFontFamilyProxy() override;

  // IDWriteFontFamily:
  HRESULT STDMETHODCALLTYPE
  GetFontCollection(IDWriteFontCollection** font_collection) override;
  UINT32 STDMETHODCALLTYPE GetFontCount() override;
  HRESULT STDMETHODCALLTYPE GetFont(UINT32 index, IDWriteFont** font) override;
  HRESULT STDMETHODCALLTYPE
  GetFamilyNames(IDWriteLocalizedStrings** names) override;
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

  bool GetFontFromFontFace(IDWriteFontFace* font_face, IDWriteFont** font);

  void SetName(const base::string16& family_name);

  const base::string16& GetName();

  bool IsLoaded();

 protected:
  bool LoadFamily();

 private:
  UINT32 family_index_;
  base::string16 family_name_;
  Microsoft::WRL::ComPtr<DWriteFontCollectionProxy> proxy_collection_;
  Microsoft::WRL::ComPtr<IDWriteFontFamily> family_;
  Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names_;

  DISALLOW_ASSIGN(DWriteFontFamilyProxy);
};

// Implements the DirectWrite font file enumerator interface, backed by a list
// of font files.
class FontFileEnumerator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteFontFileEnumerator> {
 public:
  FontFileEnumerator();
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

  DISALLOW_ASSIGN(FontFileEnumerator);
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

  DISALLOW_ASSIGN(FontFileStream);
};

}  // namespace content
#endif  // CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_FONT_PROXY_WIN_H_
