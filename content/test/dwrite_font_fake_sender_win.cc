// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/dwrite_font_fake_sender_win.h"

#include <dwrite.h>
#include <shlobj.h>

#include <memory>

#include "base/bind.h"

namespace content {

void AddFamily(const base::FilePath& font_path,
               const base::string16& family_name,
               const base::string16& base_family_name,
               FakeFontCollection* collection) {
  collection->AddFont(family_name)
      .AddFamilyName(L"en-us", family_name)
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
  std::vector<base::char16> font_path_chars;
  font_path_chars.resize(MAX_PATH);
  SHGetSpecialFolderPath(nullptr /*hwndOwner - reserved*/,
                         font_path_chars.data(), CSIDL_FONTS,
                         FALSE /*fCreate*/);
  base::FilePath font_path(base::string16(font_path_chars.data()));
  std::unique_ptr<FakeFontCollection> fake_collection =
      std::make_unique<FakeFontCollection>();
  AddFamily(font_path, L"Arial", L"arial", fake_collection.get());
  AddFamily(font_path, L"Courier New", L"cour", fake_collection.get());
  AddFamily(font_path, L"Times New Roman", L"times", fake_collection.get());
  return base::BindRepeating(&CreateFakeCollectionRemote,
                             std::move(fake_collection));
}

FakeFont::FakeFont(const base::string16& name) : font_name_(name) {}

FakeFont::FakeFont(FakeFont&& other) = default;

FakeFont::~FakeFont() = default;

FakeFontCollection::FakeFontCollection() = default;

FakeFont& FakeFontCollection::AddFont(const base::string16& font_name) {
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

void FakeFontCollection::FindFamily(const base::string16& family_name,
                                    FindFamilyCallback callback) {
  message_types_.push_back(MessageType::kFindFamily);
  for (size_t n = 0; n < fonts_.size(); n++) {
    if (_wcsicmp(family_name.data(), fonts_[n].font_name_.data()) == 0) {
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
      family_names.emplace_back(base::in_place, name.first, name.second);
    }
  }
  std::move(callback).Run(std::move(family_names));
}

void FakeFontCollection::GetFontFiles(uint32_t family_index,
                                      GetFontFilesCallback callback) {
  message_types_.push_back(MessageType::kGetFontFiles);
  std::vector<base::FilePath> file_paths;
  std::vector<base::File> file_handles;
  if (family_index < fonts_.size()) {
    file_paths = fonts_[family_index].file_paths_;
    for (auto& file : fonts_[family_index].file_handles_)
      file_handles.emplace_back(file.Duplicate());
  }
  std::move(callback).Run(file_paths, std::move(file_handles));
}

void FakeFontCollection::MapCharacters(
    const base::string16& text,
    blink::mojom::DWriteFontStylePtr font_style,
    const base::string16& locale_name,
    uint32_t reading_direction,
    const base::string16& base_family_name,
    MapCharactersCallback callback) {
  message_types_.push_back(MessageType::kMapCharacters);
  std::move(callback).Run(blink::mojom::MapCharactersResult::New(
      0, fonts_[0].font_name(), 1, 1.0,
      blink::mojom::DWriteFontStyle::New(DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL)));
}

void FakeFontCollection::MatchUniqueFont(const base::string16& unique_font_name,
                                         MatchUniqueFontCallback callback) {}

void FakeFontCollection::GetUniqueFontLookupMode(
    GetUniqueFontLookupModeCallback callback) {}

void FakeFontCollection::GetUniqueNameLookupTable(
    GetUniqueNameLookupTableCallback callback) {}

void FakeFontCollection::GetUniqueNameLookupTableIfAvailable(
    GetUniqueNameLookupTableIfAvailableCallback callback) {}

void FakeFontCollection::FallbackFamilyAndStyleForCodepoint(
    const std::string& base_family_name,
    const std::string& locale_name,
    uint32_t codepoint,
    FallbackFamilyAndStyleForCodepointCallback callback) {}

FakeFontCollection::~FakeFontCollection() = default;

}  // namespace content
