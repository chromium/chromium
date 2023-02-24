// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"

#include <dwrite.h>
#include <shlobj.h>
#include <wrl.h>

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "content/test/dwrite_font_fake_sender_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

class DWriteFontProxyUnitTest : public testing::Test {
 public:
  DWriteFontProxyUnitTest() {
    fake_collection_ = std::make_unique<FakeFontCollection>();
    SetupFonts(fake_collection_.get());
    DWriteFontCollectionProxy::Create(&collection_, factory.Get(),
                                      fake_collection_->CreateRemote());
    EXPECT_TRUE(collection_.Get());
  }

  ~DWriteFontProxyUnitTest() override {
    if (collection_)
      collection_->Unregister();
  }

  static void SetupFonts(FakeFontCollection* fonts) {
    fonts->AddFont(u"Aardvark")
        .AddFamilyName(u"en-us", u"Aardvark")
        .AddFamilyName(u"de-de", u"Erdferkel")
        .AddFilePath(base::FilePath(L"X:\\Nonexistent\\Folder\\Aardvark.ttf"));
    FakeFont& arial =
        fonts->AddFont(u"Arial").AddFamilyName(u"en-us", u"Arial");
    for (auto& path : arial_font_files)
      arial.AddFilePath(base::FilePath(path));
    fonts->AddFont(u"Times New Roman")
        .AddFamilyName(u"en-us", u"Times New Roman")
        .AddFilePath(base::FilePath(L"X:\\Nonexistent\\Folder\\Times.ttf"));
  }

  static void SetUpTestCase() {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        &factory);

    std::vector<wchar_t> font_path;
    font_path.resize(MAX_PATH);
    SHGetSpecialFolderPath(nullptr /* hwndOwner - reserved */, font_path.data(),
                           CSIDL_FONTS, FALSE /* fCreate*/);
    std::wstring arial;
    arial.append(font_path.data()).append(L"\\arial.ttf");
    std::wstring arialbd;
    arialbd.append(font_path.data()).append(L"\\arialbd.ttf");
    arial_font_files.push_back(arial);
    arial_font_files.push_back(arialbd);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeFontCollection> fake_collection_;
  mswr::ComPtr<DWriteFontCollectionProxy> collection_;

  static std::vector<std::wstring> arial_font_files;
  static mswr::ComPtr<IDWriteFactory> factory;
};
std::vector<std::wstring> DWriteFontProxyUnitTest::arial_font_files;
mswr::ComPtr<IDWriteFactory> DWriteFontProxyUnitTest::factory;

TEST_F(DWriteFontProxyUnitTest, GetFontFamilyCount) {
  UINT32 family_count = collection_->GetFontFamilyCount();

  EXPECT_EQ(3u, family_count);
  ASSERT_EQ(1u, fake_collection_->MessageCount());
  EXPECT_EQ(FakeFontCollection::MessageType::kGetFamilyCount,
            fake_collection_->GetMessageType(0));

  // Calling again should not cause another message to be sent.
  family_count = collection_->GetFontFamilyCount();
  EXPECT_EQ(3u, family_count);
  ASSERT_EQ(1u, fake_collection_->MessageCount());
}

TEST_F(DWriteFontProxyUnitTest, FindFamilyNameShouldFindFamily) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Arial", &index, &exists);

  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1u, index);
  EXPECT_TRUE(exists);
  ASSERT_EQ(2u, fake_collection_->MessageCount());
  EXPECT_EQ(FakeFontCollection::MessageType::kFindFamily,
            fake_collection_->GetMessageType(0));
  EXPECT_EQ(FakeFontCollection::MessageType::kGetFamilyCount,
            fake_collection_->GetMessageType(1));
}

TEST_F(DWriteFontProxyUnitTest, FindFamilyNameShouldReturnUINTMAXWhenNotFound) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Not a font", &index, &exists);

  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(UINT32_MAX, index);
  EXPECT_FALSE(exists);
  ASSERT_EQ(1u, fake_collection_->MessageCount());
  EXPECT_EQ(FakeFontCollection::MessageType::kFindFamily,
            fake_collection_->GetMessageType(0));
}

TEST_F(DWriteFontProxyUnitTest, FindFamilyNameShouldNotSendDuplicateIPC) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Arial", &index, &exists);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(2u, fake_collection_->MessageCount());

  hr = collection_->FindFamilyName(L"Arial", &index, &exists);

  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2u, fake_collection_->MessageCount());
}

TEST_F(DWriteFontProxyUnitTest, GetFontFamilyShouldCreateFamily) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Arial", &index, &exists);
  ASSERT_EQ(2u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteFontFamily> family;
  hr = collection_->GetFontFamily(2, &family);

  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2u, fake_collection_->MessageCount());
  EXPECT_NE(nullptr, family.Get());
}

TEST_F(DWriteFontProxyUnitTest, PrewarmFamilyShouldCreateFamily) {
  collection_->InitializePrewarmerForTesting(fake_collection_->CreateRemote());

  collection_->PrewarmFamily("Arial");
  // Run posted tasks in |ThreadPool|.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(3u, fake_collection_->MessageCount());

  // |FindFamilyName| should not make any mojo calls, because |PrewarmFamily|
  // should have already created the family.
  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  HRESULT hr = collection_->FindFamilyName(L"Arial", &index, &exists);
  EXPECT_EQ(S_OK, hr);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(3u, fake_collection_->MessageCount());
}

void CheckLocale(const std::wstring& locale_name,
                 const std::wstring& expected_value,
                 IDWriteLocalizedStrings* strings) {
  UINT32 locale_index = 0;
  BOOL locale_exists = FALSE;
  strings->FindLocaleName(locale_name.data(), &locale_index, &locale_exists);
  EXPECT_TRUE(locale_exists);

  UINT32 name_length = 0;
  strings->GetLocaleNameLength(locale_index, &name_length);
  EXPECT_EQ(locale_name.size(), name_length);
  std::wstring actual_name;
  name_length++;
  actual_name.resize(name_length);
  strings->GetLocaleName(locale_index, const_cast<WCHAR*>(actual_name.data()),
                         name_length);
  EXPECT_STREQ(locale_name.c_str(), actual_name.c_str());

  UINT32 string_length = 0;
  strings->GetStringLength(locale_index, &string_length);
  EXPECT_EQ(expected_value.size(), string_length);
  std::wstring actual_value;
  string_length++;
  actual_value.resize(string_length);
  strings->GetString(locale_index, const_cast<WCHAR*>(actual_value.data()),
                     string_length);
  EXPECT_STREQ(expected_value.c_str(), actual_value.c_str());
}

TEST_F(DWriteFontProxyUnitTest, GetFamilyNames) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Aardvark", &index, &exists);
  ASSERT_EQ(2u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteFontFamily> family;
  hr = collection_->GetFontFamily(index, &family);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteLocalizedStrings> names;
  hr = family->GetFamilyNames(&names);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(3u, fake_collection_->MessageCount());
  EXPECT_EQ(FakeFontCollection::MessageType::kGetFamilyNames,
            fake_collection_->GetMessageType(2));

  EXPECT_EQ(2u, names->GetCount());
  UINT32 locale_index = 0;
  BOOL locale_exists = FALSE;
  hr = names->FindLocaleName(L"fr-fr", &locale_index, &locale_exists);
  EXPECT_EQ(S_OK, hr);
  EXPECT_FALSE(locale_exists);

  CheckLocale(L"en-us", L"Aardvark", names.Get());
  CheckLocale(L"de-de", L"Erdferkel", names.Get());

  std::wstring unused;
  unused.resize(25);
  hr = names->GetLocaleName(15234, const_cast<WCHAR*>(unused.data()),
                            unused.size() - 1);
  EXPECT_FALSE(SUCCEEDED(hr));
}

TEST_F(DWriteFontProxyUnitTest, GetFontCollection) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Arial", &index, &exists);
  ASSERT_EQ(2u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteFontFamily> family;
  hr = collection_->GetFontFamily(2, &family);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteFontCollection> returned_collection;
  hr = family->GetFontCollection(&returned_collection);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2u, fake_collection_->MessageCount());
  EXPECT_EQ(collection_.Get(), returned_collection.Get());
}

TEST_F(DWriteFontProxyUnitTest, GetFamilyNamesShouldNotIPCAfterLoadingFamily) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  collection_->FindFamilyName(L"Arial", &index, &exists);
  mswr::ComPtr<IDWriteFontFamily> family;
  collection_->GetFontFamily(index, &family);
  family->GetFontCount();
  EXPECT_EQ(3u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteLocalizedStrings> names;
  hr = family->GetFamilyNames(&names);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(3u, fake_collection_->MessageCount());
}

TEST_F(DWriteFontProxyUnitTest,
       GetFontFamilyShouldNotCreateFamilyWhenIndexIsInvalid) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  hr = collection_->FindFamilyName(L"Arial", &index, &exists);
  ASSERT_EQ(2u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteFontFamily> family;
  hr = collection_->GetFontFamily(1654, &family);

  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(2u, fake_collection_->MessageCount());
}

TEST_F(DWriteFontProxyUnitTest, LoadingFontFamily) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  collection_->FindFamilyName(L"Arial", &index, &exists);
  mswr::ComPtr<IDWriteFontFamily> family;
  collection_->GetFontFamily(index, &family);
  ASSERT_EQ(2u, fake_collection_->MessageCount());

  UINT32 font_count = family->GetFontCount();
  EXPECT_LT(0u, font_count);
  EXPECT_EQ(3u, fake_collection_->MessageCount());
  EXPECT_EQ(FakeFontCollection::MessageType::kGetFontFileHandles,
            fake_collection_->GetMessageType(2));
  mswr::ComPtr<IDWriteFont> font;
  hr = family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                    DWRITE_FONT_STRETCH_NORMAL,
                                    DWRITE_FONT_STYLE_NORMAL, &font);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(3u, fake_collection_->MessageCount());
  mswr::ComPtr<IDWriteFont> font2;
  hr = family->GetFont(0, &font2);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(3u, fake_collection_->MessageCount());
  mswr::ComPtr<IDWriteFontList> matching_fonts;
  hr = family->GetMatchingFonts(DWRITE_FONT_WEIGHT_NORMAL,
                                DWRITE_FONT_STRETCH_NORMAL,
                                DWRITE_FONT_STYLE_NORMAL, &matching_fonts);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(3u, fake_collection_->MessageCount());
  EXPECT_NE(nullptr, matching_fonts.Get());
}

TEST_F(DWriteFontProxyUnitTest, GetFontFromFontFaceShouldFindFont) {
  HRESULT hr;

  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  collection_->FindFamilyName(L"Arial", &index, &exists);
  mswr::ComPtr<IDWriteFontFamily> family;
  collection_->GetFontFamily(index, &family);

  mswr::ComPtr<IDWriteFont> font;
  family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                               DWRITE_FONT_STRETCH_NORMAL,
                               DWRITE_FONT_STYLE_NORMAL, &font);

  mswr::ComPtr<IDWriteFontFace> font_face;
  hr = font->CreateFontFace(&font_face);
  ASSERT_TRUE(SUCCEEDED(hr));
  ASSERT_EQ(3u, fake_collection_->MessageCount());

  mswr::ComPtr<IDWriteFont> found_font;
  collection_->GetFontFromFontFace(font_face.Get(), &found_font);
  EXPECT_NE(nullptr, found_font.Get());
  EXPECT_EQ(3u, fake_collection_->MessageCount());
}

TEST_F(DWriteFontProxyUnitTest, TestCustomFontFiles) {
  FakeFontCollection fonts;
  FakeFont& arial = fonts.AddFont(u"Arial").AddFamilyName(u"en-us", u"Arial");
  for (auto& path : arial_font_files) {
    base::File file(base::FilePath(path),
                    base::File::FLAG_OPEN | base::File::FLAG_READ |
                        base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    arial.AddFileHandle(std::move(file));
  }
  mswr::ComPtr<DWriteFontCollectionProxy> collection;
  DWriteFontCollectionProxy::Create(&collection, factory.Get(),
                                    fonts.CreateRemote());

  // Check that we can get the font family and match a font.
  UINT32 index = UINT_MAX;
  BOOL exists = FALSE;
  collection->FindFamilyName(L"Arial", &index, &exists);
  mswr::ComPtr<IDWriteFontFamily> family;
  collection->GetFontFamily(index, &family);

  mswr::ComPtr<IDWriteFont> font;
  HRESULT hr = family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                            DWRITE_FONT_STRETCH_NORMAL,
                                            DWRITE_FONT_STYLE_NORMAL, &font);

  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_NE(nullptr, font.Get());

  mswr::ComPtr<IDWriteFontFace> font_face;
  hr = font->CreateFontFace(&font_face);
  EXPECT_TRUE(SUCCEEDED(hr));
}

}  // namespace

}  // namespace content
