// Copyright 2015 The Chromium Authors
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
  explicit FakeFont(const std::u16string& name);

  FakeFont& operator=(const FakeFont&) = delete;

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

  FakeFont& AddFamilyName(const std::u16string& locale,
                          const std::u16string& family_name) {
    family_names_.emplace_back(locale, family_name);
    return *this;
  }

  const std::u16string& font_name() { return font_name_; }

 private:
  friend FakeFontCollection;
  std::u16string font_name_;
  std::vector<base::FilePath> file_paths_;
  std::vector<base::File> file_handles_;
  std::vector<std::pair<std::u16string, std::u16string>> family_names_;
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
    kGetFontFileHandles,
    kMapCharacters
  };
  FakeFontCollection();

  FakeFontCollection(const FakeFontCollection&) = delete;
  FakeFontCollection& operator=(const FakeFontCollection&) = delete;

  ~FakeFontCollection() override;

  FakeFont& AddFont(const std::u16string& font_name);

  size_t MessageCount();
  MessageType GetMessageType(size_t id);

  mojo::PendingRemote<blink::mojom::DWriteFontProxy> CreateRemote();

 protected:
  // blink::mojom::DWriteFontProxy:
  void FindFamily(const std::u16string& family_name,
                  FindFamilyCallback callback) override;
  void GetFamilyCount(GetFamilyCountCallback callback) override;
  void GetFamilyNames(uint32_t family_index,
                      GetFamilyNamesCallback callback) override;
  void GetFontFileHandles(uint32_t family_index,
                          GetFontFileHandlesCallback callback) override;
  void MapCharacters(const std::u16string& text,
                     blink::mojom::DWriteFontStylePtr font_style,
                     const std::u16string& locale_name,
                     uint32_t reading_direction,
                     const std::u16string& base_family_name,
                     MapCharactersCallback callback) override;
  void MatchUniqueFont(const std::u16string& unique_font_name,
                       MatchUniqueFontCallback callback) override;

 private:
  std::vector<FakeFont> fonts_;

  std::vector<MessageType> message_types_;

  mojo::ReceiverSet<blink::mojom::DWriteFontProxy> receivers_;
};

}  // namespace content

#endif  // CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_
