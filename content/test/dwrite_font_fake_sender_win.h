// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_
#define CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_

#include <stddef.h>
#include <stdint.h>
#include <wrl.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom.h"

namespace content {

class FakeFontCollection;

// Creates a new FakeFontCollection, seeded with some basic data, and returns a
// Sender that can be used to interact with the collection.
base::RepeatingCallback<
    mojo::PendingRemote<blink::mojom::DWriteFontProxy>(void)>
CreateFakeCollectionSender();

// Helper class for describing a font object. Use FakeFontCollection instead.
class FakeFont {
 public:
  explicit FakeFont(const base::string16& name);

  FakeFont(FakeFont&& other);

  ~FakeFont();

  FakeFont& AddFilePath(const base::FilePath& file_path) {
    file_paths_.push_back(file_path);
    return *this;
  }

  FakeFont& AddFileHandle(base::File handle) {
    file_handles_.push_back(std::move(handle));
    return *this;
  }

  FakeFont& AddFamilyName(const base::string16& locale,
                          const base::string16& family_name) {
    family_names_.emplace_back(locale, family_name);
    return *this;
  }

  const base::string16& font_name() { return font_name_; }

 private:
  friend FakeFontCollection;
  base::string16 font_name_;
  std::vector<base::FilePath> file_paths_;
  std::vector<base::File> file_handles_;
  std::vector<std::pair<base::string16, base::string16>> family_names_;

  DISALLOW_ASSIGN(FakeFont);
};

// Implements a font collection that supports interaction through sending IPC
// messages from dwrite_font_proxy_messages.h.
// Usage:
//   Create a new FakeFontCollection.
//   Call AddFont() to add fonts.
//     For each font, call methods on the font to configure the font.
//     Note: the indices of the fonts will correspond to the order they were
//         added. The collection will not sort or reorder fonts in any way.
//   Call GetSender()/GetTrackingSender() to obtain an IPC::Sender.
//   Call Send() with DWriteFontProxyMsg_* to interact with the collection.
//   Call MessageCount()/GetIpcMessage() to inspect sent messages.
//
// The internal code flow for GetSender()/Send() is as follows:
//   GetSender() returns a new FakeSender, pointing back to the collection.
//   FakeSender::Send() will create a new ReplySender and call
//       ReplySender::OnMessageReceived()
//   ReplySender::OnMessageReceived() contains the message map, which will
//       internally call ReplySender::On*() and ReplySender::Send()
//   ReplySender::Send() will save the reply message, to be used later.
//   FakeSender::Send() will retrieve the reply message and save the output.
class FakeFontCollection : public blink::mojom::DWriteFontProxy {
 public:
  enum class MessageType {
    kFindFamily,
    kGetFamilyCount,
    kGetFamilyNames,
    kGetFontFiles,
    kMapCharacters
  };
  FakeFontCollection();
  ~FakeFontCollection() override;

  FakeFont& AddFont(const base::string16& font_name);

  size_t MessageCount();
  MessageType GetMessageType(size_t id);

  mojo::PendingRemote<blink::mojom::DWriteFontProxy> CreateRemote();

 protected:
  // blink::mojom::DWriteFontProxy:
  void FindFamily(const base::string16& family_name,
                  FindFamilyCallback callback) override;
  void GetFamilyCount(GetFamilyCountCallback callback) override;
  void GetFamilyNames(uint32_t family_index,
                      GetFamilyNamesCallback callback) override;
  void GetFontFiles(uint32_t family_index,
                    GetFontFilesCallback callback) override;
  void MapCharacters(const base::string16& text,
                     blink::mojom::DWriteFontStylePtr font_style,
                     const base::string16& locale_name,
                     uint32_t reading_direction,
                     const base::string16& base_family_name,
                     MapCharactersCallback callback) override;
  void MatchUniqueFont(const base::string16& unique_font_name,
                       MatchUniqueFontCallback callback) override;
  void GetUniqueFontLookupMode(
      GetUniqueFontLookupModeCallback callback) override;
  void GetUniqueNameLookupTableIfAvailable(
      GetUniqueNameLookupTableIfAvailableCallback callback) override;
  void GetUniqueNameLookupTable(
      GetUniqueNameLookupTableCallback callback) override;
  void FallbackFamilyAndStyleForCodepoint(
      const std::string& base_family_name,
      const std::string& locale_name,
      uint32_t codepoint,
      FallbackFamilyAndStyleForCodepointCallback callback) override;

 private:
  std::vector<FakeFont> fonts_;

  std::vector<MessageType> message_types_;

  mojo::ReceiverSet<blink::mojom::DWriteFontProxy> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeFontCollection);
};

}  // namespace content

#endif  // CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_
