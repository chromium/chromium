// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/dwrite_font_fake_sender_win.h"

#include <dwrite.h>
#include <shlobj.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace content {

void AddFamily(const base::FilePath& font_path,
               const std::u16string& family_name,
               const std::wstring& base_family_name,
               FakeFontCollection* collection) {
  collection->AddFont(family_name)
      .AddFamilyName(u"en-us", family_name)
      .AddFilePath(font_path.Append(L"\\" + base_family_name + L".ttf"))
      .AddFilePath(font_path.Append(L"\\" + base_family_name + L"bd.ttf"))
      .AddFilePath(font_path.Append(L"\\" + base_family_name + L"bi.ttf"))
      .AddFilePath(font_path.Append(L"\\" + base_family_name + L"i.ttf"));
}

mojo::PendingRemote<blink::mojom::DWriteFontProxy> CreateFakeCollectionRemote(
    const std::unique_ptr<FakeFontCollection>& collection) {
  return collection->CreateRemote();
}

base::RepeatingCallback<
    mojo::PendingRemote<blink::mojom::DWriteFontProxy>(void)>
CreateFakeCollectionSender() {
  std::vector<wchar_t> font_path_chars;
  font_path_chars.resize(MAX_PATH);
  SHGetSpecialFolderPath(nullptr /*hwndOwner - reserved*/,
                         font_path_chars.data(), CSIDL_FONTS,
                         FALSE /*fCreate*/);
  base::FilePath font_path(std::wstring(font_path_chars.data()));
  std::unique_ptr<FakeFontCollection> fake_collection =
      std::make_unique<FakeFontCollection>();
  AddFamily(font_path, u"Arial", L"arial", fake_collection.get());
  AddFamily(font_path, u"Courier New", L"cour", fake_collection.get());
  AddFamily(font_path, u"Times New Roman", L"times", fake_collection.get());
  return base::BindRepeating(&CreateFakeCollectionRemote,
                             std::move(fake_collection));
}

FakeFont::FakeFont(const std::u16string& name) : font_name_(name) {}

FakeFont::FakeFont(FakeFont&& other) = default;

FakeFont::~FakeFont() = default;

FakeFontCollection::FakeFontCollection() = default;

FakeFont& FakeFontCollection::AddFont(const std::u16string& font_name) {
  fonts_.emplace_back(font_name);
  return fonts_.back();
}

mojo::PendingRemote<blink::mojom::DWriteFontProxy>
FakeFontCollection::CreateRemote() {
  mojo::PendingRemote<blink::mojom::DWriteFontProxy> proxy;
  receivers_.Add(this, proxy.InitWithNewPipeAndPassReceiver());
  return proxy;
}

size_t FakeFontCollection::MessageCount() {
  return message_types_.size();
}

FakeFontCollection::MessageType FakeFontCollection::GetMessageType(size_t id) {
  return message_types_[id];
}

void FakeFontCollection::FindFamily(const std::u16string& family_name,
                                    FindFamilyCallback callback) {
  message_types_.push_back(MessageType::kFindFamily);
  for (size_t n = 0; n < fonts_.size(); n++) {
    if (base::EqualsCaseInsensitiveASCII(family_name.data(),
                                         fonts_[n].font_name_.data())) {
      std::move(callback).Run(n);
      return;
    }
  }
  std::move(callback).Run(UINT32_MAX);
}

void FakeFontCollection::GetFamilyCount(GetFamilyCountCallback callback) {
  message_types_.push_back(MessageType::kGetFamilyCount);
  std::move(callback).Run(fonts_.size());
}

void FakeFontCollection::GetFamilyNames(uint32_t family_index,
                                        GetFamilyNamesCallback callback) {
  message_types_.push_back(MessageType::kGetFamilyNames);
  std::vector<blink::mojom::DWriteStringPairPtr> family_names;
  if (family_index < fonts_.size()) {
    for (const auto& name : fonts_[family_index].family_names_) {
      family_names.emplace_back(std::in_place, name.first, name.second);
    }
  }
  std::move(callback).Run(std::move(family_names));
}

void FakeFontCollection::GetFontFileHandles(
    uint32_t family_index,
    GetFontFileHandlesCallback callback) {
  message_types_.push_back(MessageType::kGetFontFileHandles);
  std::vector<base::File> file_handles;
  if (family_index < fonts_.size()) {
    for (const auto& font_path : fonts_[family_index].file_paths_) {
      base::File file(base::FilePath(font_path),
                      base::File::FLAG_OPEN | base::File::FLAG_READ |
                          base::File::FLAG_WIN_EXCLUSIVE_WRITE);
      if (file.IsValid()) {
        file_handles.emplace_back(std::move(file));
      }
    }
    for (auto& file : fonts_[family_index].file_handles_)
      file_handles.emplace_back(file.Duplicate());
  }
  std::move(callback).Run(std::move(file_handles));
}

void FakeFontCollection::MapCharacters(
    const std::u16string& text,
    blink::mojom::DWriteFontStylePtr font_style,
    const std::u16string& locale_name,
    uint32_t reading_direction,
    const std::u16string& base_family_name,
    MapCharactersCallback callback) {
  message_types_.push_back(MessageType::kMapCharacters);
  std::move(callback).Run(blink::mojom::MapCharactersResult::New(
      0, fonts_[0].font_name(), 1, 1.0,
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL)));
}

void FakeFontCollection::MatchUniqueFont(const std::u16string& unique_font_name,
                                         MatchUniqueFontCallback callback) {}

FakeFontCollection::~FakeFontCollection() = default;

}  // namespace content
