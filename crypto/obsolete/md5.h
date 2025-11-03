// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_OBSOLETE_MD5_H_
#define CRYPTO_OBSOLETE_MD5_H_

#include <array>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/digest.h"

namespace crypto::obsolete {
static constexpr size_t kMd5Size = 16;
class Md5;
}

namespace ash::printing {
crypto::obsolete::Md5 MakeMd5HasherForPrinterConfigurer();
crypto::obsolete::Md5 MakeMd5HasherForUsbPrinterUtil();
crypto::obsolete::Md5 MakeMd5HasherForZeroconf();
std::string PolicyPrinterId(const std::string& json);
std::string ServerPrinterId(const std::string& url);
}  // namespace ash::printing

namespace android_tools {
crypto::obsolete::Md5 MakeMd5HasherForMd5sumTool();
}

namespace autofill {
crypto::obsolete::Md5 MakeMd5HasherForPasswordRequirementsSpec();
}

namespace base {
std::array<uint8_t, crypto::obsolete::kMd5Size> Md5ForWinInspectionResultsCache(
    base::span<const uint8_t> payload);
}

namespace blink {
uint32_t MD5Hash32ForBackgroundTracingHelper(std::string_view str);
}

namespace bookmarks {
class BookmarkCodec;
}  // namespace bookmarks

namespace cachetool {
crypto::obsolete::Md5 MakeMd5HasherForCachetools();
}

namespace content {
std::string Md5AsHexForWebTestPixels(base::span<const uint8_t> pixels);
}

namespace content_suggestions_tile_saver {
std::string Md5AsHexForFaviconUrl(std::string_view url);
}

namespace devtools {
std::string Md5OfUrlAsHexForDevTools(std::string_view url);
}

namespace display {
std::string Md5AsHexForEdid(std::string_view data);
}

namespace drive {
crypto::obsolete::Md5 MakeMd5HasherForDriveFsAccount();
}

namespace drive::util {
crypto::obsolete::Md5 MakeMd5HasherForDriveApi();
}

namespace extensions::image_writer {
crypto::obsolete::Md5 MakeMd5HasherForImageWriter();
}

namespace history {
std::string Md5AsHexForTopSites(std::string_view url_spec);
}

namespace hunspell {
std::array<uint8_t, crypto::obsolete::kMd5Size> Md5ForBdict(
    base::span<const uint8_t> data);
std::array<uint8_t, crypto::obsolete::kMd5Size> Md5ForBdictWriter(
    base::span<const uint8_t> data);
}  // namespace hunspell

namespace media::test {
crypto::obsolete::Md5 MakeMd5HasherForVideoFrameValidation();
}

namespace nearby {
std::array<uint8_t, crypto::obsolete::kMd5Size> Md5ForNearby(
    std::string_view input);
}

namespace net {
crypto::obsolete::Md5 MakeMd5HasherForHttpVaryData();
}

namespace performance_manager {
std::string Md5AsHexForDatabaseKey(std::string_view input);
}

namespace policy {
crypto::obsolete::Md5 MakeMd5HasherForPolicyEventId();
}

namespace reading_list {
std::string Md5AsHexForOfflineUrlUtils(std::string_view url);
}

namespace remoting {
std::string GetHostHash();
}

namespace safe_browsing {
std::string Md5AsHexForBodyDigest(std::string_view data);
}

namespace segmentation_platform {
std::array<uint8_t, crypto::obsolete::kMd5Size> Md5ForUrlId(
    std::string_view data);
}

namespace shell_util {
std::string Md5AsBase32ForUserSpecificRegistrySuffix(std::string_view str);
}

namespace spellcheck {
std::string Md5AsHexForDictionaryChecksum(std::string_view data);
}

namespace spotlight {
std::array<uint8_t, crypto::obsolete::kMd5Size> Md5ForSpotlightId(
    std::string_view data);
}

namespace trusted_vault {
std::string MD5StringForTrustedVault(const std::string& local_trusted_value);
}

namespace visitedlink {
crypto::obsolete::Md5 MakeMd5HasherForVisitedLink();
}

namespace web_app::internals {
crypto::obsolete::Md5 MakeMd5HasherForWebAppShortcutIcon();
std::wstring Md5AsHexForUninstall(const std::wstring& data);
}

namespace crypto::obsolete {

// This class is used for computing MD5 hashes, either one-shot via Md5::Hash(),
// or streaming via constructing an Md5 instance, calling Update(), then calling
// Finish(). It cannot be constructed except by friend classes, and to become a
// friend class you must talk to a member of //CRYPTO_OWNERS. You should not use
// MD5 in new production code.
class CRYPTO_EXPORT Md5 {
 public:
  static constexpr size_t kSize = kMd5Size;

  Md5(const Md5& other);
  Md5(Md5&& other);
  Md5& operator=(const Md5& other);
  Md5& operator=(Md5&& other);
  ~Md5();

  void Update(std::string_view data);
  void Update(base::span<const uint8_t> data);

  void Finish(base::span<uint8_t, kSize> result);
  std::array<uint8_t, kSize> Finish();

  static Md5 MakeMd5HasherForTesting();
  static std::array<uint8_t, kSize> HashForTesting(
      base::span<const uint8_t> data);

 private:
  FRIEND_TEST_ALL_PREFIXES(Md5Test, KnownAnswer);

  // The friends listed here are the areas required to continue using MD5 for
  // compatibility with existing specs, on-disk data, or similar.
  friend Md5 android_tools::MakeMd5HasherForMd5sumTool();
  friend uint32_t blink::MD5Hash32ForBackgroundTracingHelper(
      std::string_view str);
  friend Md5 cachetool::MakeMd5HasherForCachetools();
  friend std::string content::Md5AsHexForWebTestPixels(
      base::span<const uint8_t> pixels);
  friend Md5 drive::MakeMd5HasherForDriveFsAccount();
  friend Md5 drive::util::MakeMd5HasherForDriveApi();
  friend Md5 extensions::image_writer::MakeMd5HasherForImageWriter();
  friend std::array<uint8_t, kSize> hunspell::Md5ForBdict(
      base::span<const uint8_t> data);
  friend std::array<uint8_t, kSize> hunspell::Md5ForBdictWriter(
      base::span<const uint8_t> data);
  friend std::array<uint8_t, kSize> nearby::Md5ForNearby(
      std::string_view input);
  friend Md5 policy::MakeMd5HasherForPolicyEventId();
  friend std::string remoting::GetHostHash();
  friend std::string safe_browsing::Md5AsHexForBodyDigest(
      std::string_view data);
  friend std::string shell_util::Md5AsBase32ForUserSpecificRegistrySuffix(
      std::string_view str);
  friend std::string spellcheck::Md5AsHexForDictionaryChecksum(
      std::string_view data);

  // TODO(b/298652869): get rid of these.
  friend Md5 ash::printing::MakeMd5HasherForPrinterConfigurer();
  friend Md5 ash::printing::MakeMd5HasherForUsbPrinterUtil();
  friend Md5 ash::printing::MakeMd5HasherForZeroconf();
  friend std::string ash::printing::PolicyPrinterId(const std::string& json);
  friend std::string ash::printing::ServerPrinterId(const std::string& url);

  // TODO(https://crbug.com/433545115): get rid of this.
  friend Md5 autofill::MakeMd5HasherForPasswordRequirementsSpec();

  // TODO(https://crbug.com/450285252): get rid of this.
  friend std::array<uint8_t, Md5::kSize> base::Md5ForWinInspectionResultsCache(
      base::span<const uint8_t> payload);

  // TODO(https://crbug.com/426243026): get rid of this.
  friend class bookmarks::BookmarkCodec;

  // TODO(crbug.com/454958766): get rid of this.
  friend std::string content_suggestions_tile_saver::Md5AsHexForFaviconUrl(
      std::string_view url);

  // TODO(crbug.com/454363517): get rid of this.
  friend std::string devtools::Md5OfUrlAsHexForDevTools(std::string_view url);

  // TODO(crbug.com/454630854): get rid of this.
  friend std::string display::Md5AsHexForEdid(std::string_view data);

  // TODO(https://crbug.com/454354275): get rid of this.
  friend std::string history::Md5AsHexForTopSites(std::string_view url_spec);

  // TODO(https://crbug.com/428022614): get rid of this.
  friend Md5 media::test::MakeMd5HasherForVideoFrameValidation();

  // TODO(https://crbug.com/419853200): get rid of this.
  friend Md5 net::MakeMd5HasherForHttpVaryData();

  // TODO(crbug.com/454931298): get rid of this.
  friend std::string performance_manager::Md5AsHexForDatabaseKey(
      std::string_view input);

  // TODO(https://crbug.com/454946840): get rid of this.
  friend std::string reading_list::Md5AsHexForOfflineUrlUtils(
      std::string_view url);

  // TODO(crbug.com/455854083): get rid of this.
  friend std::array<uint8_t, kSize> segmentation_platform::Md5ForUrlId(
      std::string_view data);

  // TODO(crbug.com/456473948): get rid of this.
  friend std::array<uint8_t, kSize> spotlight::Md5ForSpotlightId(
      std::string_view data);

  // TODO(https://crbug.com/425990763): get rid of this.
  friend std::string trusted_vault::MD5StringForTrustedVault(
      const std::string& local_trusted_value);

  // TODO(https://crbug.com/427437222): get rid of this.
  friend Md5 visitedlink::MakeMd5HasherForVisitedLink();

  // TODO(https://crbug.com/416304903): get rid of this.
  friend Md5 web_app::internals::MakeMd5HasherForWebAppShortcutIcon();
  friend std::wstring web_app::internals::Md5AsHexForUninstall(
      const std::wstring& key);

  Md5();
  static std::array<uint8_t, kSize> Hash(std::string_view data);
  static std::array<uint8_t, kSize> Hash(base::span<const uint8_t> data);

  bssl::ScopedEVP_MD_CTX ctx_;
};

}  // namespace crypto::obsolete

#endif  // CRYPTO_OBSOLETE_MD5_H_
