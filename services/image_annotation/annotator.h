// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_
#define SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/manta/anchovy/anchovy_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace image_annotation {

// The annotator communicates with the external image annotation server to
// perform image labeling at the behest of clients.
//
// Clients make requests of the annotator by providing an image "source ID"
// (which is either an image URL or hash of an image data URI) and an associated
// ImageProcessor (the interface through which the annotator can obtain image
// pixels if necessary).
//
// The annotator maintains a cache of previously-computed results, and will
// compute new results either by sending image URLs (for publicly-crawled
// images) or image pixels to the external server.
class Annotator : public mojom::Annotator {
 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void BindJsonParser(
        mojo::PendingReceiver<data_decoder::mojom::JsonParser> receiver) = 0;

    virtual std::vector<std::string> GetAcceptLanguages() = 0;
    virtual std::vector<std::string> GetTopLanguages() = 0;
    virtual void RecordLanguageMetrics(
        const std::string& page_language,
        const std::string& requested_language) = 0;
  };

  // The HTTP request header in which the API key should be transmitted.
  static constexpr char kGoogApiKeyHeader[] = "X-Goog-Api-Key";

  // The minimum side length needed to request description annotations.
  static constexpr int32_t kDescMinDimension = 150;

  // The maximum aspect ratio permitted to request description annotations.
  static constexpr double kDescMaxAspectRatio = 2.5;

  // The minimum side length needed to request icon annotations.
  static constexpr int32_t kIconMinDimension = 16;

  // The maximum side length needed to request icon annotations.
  static constexpr int32_t kIconMaxDimension = 256;

  // The maximum aspect ratio permitted to request icon annotations.
  // (Most icons are square, but something like an ellipsis / "more" menu
  // can have a long aspect ratio.)
  static constexpr double kIconMaxAspectRatio = 5.0;

  // Constructs an annotator.
  //  |pixels_server_url| : the URL to use when the annotator sends image
  //                        pixel data to get back annotations. The
  //                        annotator gracefully handles (i.e. returns
  //                        errors when constructed with) an empty server URL.
  //  |langs_server_url|  : the URL to use when the annotator requests the
  //                        set of languages supported by the server.
  //  |api_key|           : the Google API key used to authenticate
  //                        communication with the image annotation server. If
  //                        empty, no API key header will be sent.
  //  |throttle|          : the miminum amount of time to wait between sending
  //                        new HTTP requests to the image annotation server.
  //  |batch_size|        : The maximum number of image annotation requests that
  //                        should be batched into a single request to the
  //                        server.
  //  |min_ocr_confidence|: The minimum confidence value needed to return an OCR
  //                        result.
  Annotator(GURL pixels_server_url,
            GURL langs_server_url,
            std::string api_key,
            base::TimeDelta throttle,
            int batch_size,
            double min_ocr_confidence,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
            std::unique_ptr<manta::AnchovyProvider> anchovy_provider,
            std::unique_ptr<Client> client);

  Annotator(const Annotator&) = delete;
  Annotator& operator=(const Annotator&) = delete;

  ~Annotator() override;

  // Start providing behavior for the given Mojo receiver.
  void BindReceiver(mojo::PendingReceiver<mojom::Annotator> receiver);

  // mojom::Annotator:
  void AnnotateImage(const std::string& source_id,
                     const std::string& description_language_tag,
                     mojo::PendingRemote<mojom::ImageProcessor> image,
                     AnnotateImageCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AnnotatorTest, DescLanguage);
  FRIEND_TEST_ALL_PREFIXES(AnnotatorTest, ComputePreferredLanguage);
  FRIEND_TEST_ALL_PREFIXES(AnnotatorTest, FetchServerLanguages);
  FRIEND_TEST_ALL_PREFIXES(AnnotatorTest, ServerLanguagesMustContainEnglish);
  FRIEND_TEST_ALL_PREFIXES(AnnotatorTest, LanguageFallback);

  // The relevant info for a request from a client feature for a single image.
  struct ClientRequestInfo {
    ClientRequestInfo(
        mojo::PendingRemote<mojom::ImageProcessor> image_processor,
        AnnotateImageCallback callback);
    ~ClientRequestInfo();

    mojo::Remote<mojom::ImageProcessor>
        image_processor;  // The interface to use for local
                          // processing for this client.

    AnnotateImageCallback callback;  // The callback to execute when
                                     // processing has finished.
  };

  // The relevant info for a request to the image annotation server for a single
  // image.
  struct ServerRequestInfo {
    ServerRequestInfo(const std::string& source_id,
                      bool desc_requested,
                      bool icon_requested,
                      const std::string& desc_lang_tag,
                      const std::vector<uint8_t>& image_bytes);
    ServerRequestInfo(const ServerRequestInfo& other) = delete;
    ~ServerRequestInfo();

    // Use in a deque requires a move-assign operator.
    ServerRequestInfo& operator=(ServerRequestInfo&& other);
    ServerRequestInfo& operator=(const ServerRequestInfo& other) = delete;

    std::string source_id;  // The URL or hashed data URI for the image.

    bool desc_requested;  // Whether or not descriptions have been requested.
    bool icon_requested;  // Whether or not icons have been requested.
    std::string desc_lang_tag;  // The language in which descriptions have been
                                // requested.

    std::vector<uint8_t> image_bytes;  // The encoded pixel data for the image.
  };

  // The pair of (source ID, desc lang) for a client request.
  //
  // Unique request keys represent unique requests to the server (i.e. two
  // client requests that induce the same request key can be served by a single
  // server request).
  using RequestKey = std::pair<std::string, std::string>;

  // List of URL loader objects.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Returns true if the given dimensions fit the policy of the description
  // backend (i.e. the image has size / shape on which it is acceptable to run
  // the description model).
  static bool IsWithinDescPolicy(int32_t width, int32_t height);

  // Returns true if the given dimensions fit the policy of the icon
  // backend (i.e. the image has size / shape on which it is acceptable to run
  // the icon model).
  static bool IsWithinIconPolicy(int32_t width, int32_t height);

  // Constructs and returns a JSON object containing an request for the
  // given images.
  static std::string FormatJsonRequest(
      std::deque<ServerRequestInfo>::iterator begin_it,
      std::deque<ServerRequestInfo>::iterator end_it);

  // Creates a URL loader for an image annotation request.
  static std::unique_ptr<network::SimpleURLLoader> MakeRequestLoader(
      const GURL& server_url,
      const std::string& api_key);

  // Create or reuse a connection to the data decoder service for safe JSON
  // parsing.
  data_decoder::mojom::JsonParser* GetJsonParser();

  // Removes the given request, reassigning local processing if its associated
  // image processor had some ongoing.
  void RemoveRequestInfo(const RequestKey& request_key,
                         std::list<ClientRequestInfo>::iterator request_info_it,
                         bool canceled);

  // Called when a local handler returns compressed image data for the given
  // request key.
  void OnJpgImageDataReceived(
      const RequestKey& request_key,
      std::list<ClientRequestInfo>::iterator request_info_it,
      const std::vector<uint8_t>& image_bytes,
      int32_t width,
      int32_t height);

  // Called periodically to send the next batch of requests to the image
  // annotation server.
  void SendRequestBatchToServer();

  // Called when the image annotation server responds with annotations for the
  // given request keys.
  void OnServerResponseReceived(const std::set<RequestKey>& request_keys,
                                UrlLoaderList::iterator server_request_it,
                                std::unique_ptr<std::string> json_response);
  // Called once a response comes back from anchovy_provider_.
  void OnMantaResponseReceived(const RequestKey& request_key,
                               base::Time request_time,
                               base::Value::Dict dict,
                               manta::MantaStatus status);

  // Called when the data decoder service provides parsed JSON data for a server
  // response.
  void OnResponseJsonParsed(const std::set<RequestKey>& request_keys,
                            std::optional<base::Value> json_data,
                            const std::optional<std::string>& error);

  // Adds the given results to the cache (if successful) and notifies clients.
  void ProcessResults(
      const std::set<RequestKey>& request_keys,
      const std::map<std::string, mojom::AnnotateImageResultPtr>& results);

  void ProcessResult(
      const RequestKey& request_key,
      const std::map<std::string, mojom::AnnotateImageResultPtr>& results);

  std::string ComputePreferredLanguage(const std::string& page_lang) const;

  // Fetch the set of languages that the server supports.
  void FetchServerLanguages();

  // Handle the reply with the server languages.
  void OnServerLangsResponseReceived(
      const std::unique_ptr<std::string> json_response);

  // Parse the JSON from the reply with server languages.
  void OnServerLangsResponseJsonParsed(std::optional<base::Value> json_data,
                                       const std::optional<std::string>& error);

  const std::unique_ptr<manta::AnchovyProvider> anchovy_provider_;
  const std::unique_ptr<Client> client_;

  // Maps from request key to previously-obtained annotation results.
  // TODO(crbug.com/41432508): periodically clear entries from this cache.
  std::map<RequestKey, mojom::AnnotateImageResultPtr> cached_results_;

  // Maps from request key to its list of request infos (i.e. info of clients
  // that have made requests with that language and source ID).
  std::map<RequestKey, std::list<ClientRequestInfo>> request_infos_;

  // Maps from request keys of images currently being locally processed to the
  // ImageProcessors responsible for their processing.
  //
  // The value is a weak pointer to an entry in the client info list for the
  // given request key.
  //
  // Note that separate local processing will be scheduled for two requests that
  // share a source ID but differ in language. This is suboptimal; in future we
  // could share local processing among all relevant requests.
  std::map<RequestKey,
           raw_ptr<mojo::Remote<mojom::ImageProcessor>, CtnExperimental>>
      local_processors_;

  // A list of currently-ongoing HTTP requests to the image annotation server.
  UrlLoaderList ongoing_server_requests_;

  // A queue of requests for the image annotation server waiting to be made.
  std::deque<ServerRequestInfo> server_request_queue_;

  // The set of request keys for which a server request has been scheduled but
  // not yet returned to clients.
  //
  // This comprises request keys for which:
  //   - A server query has been queued (but not yet sent),
  //   - A server query is ongoing,
  //   - A server query has been returned and is being parsed.
  std::set<RequestKey> pending_requests_;

  // The request for server languages.
  std::unique_ptr<network::SimpleURLLoader> langs_url_loader_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  mojo::ReceiverSet<mojom::Annotator> receivers_;

  // Should not be used directly; GetJsonParser() should be called instead.
  mojo::Remote<data_decoder::mojom::JsonParser> json_parser_;

  // A timer used to throttle server request frequency.
  std::unique_ptr<base::RepeatingTimer> server_request_timer_;

  const GURL pixels_server_url_;
  const GURL langs_server_url_;

  const std::string api_key_;

  const int batch_size_;

  const double min_ocr_confidence_;

  // The languages that the server accepts.
  std::vector<std::string> server_languages_;

  // Used for all callbacks.
  base::WeakPtrFactory<Annotator> weak_factory_{this};
};

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_ANNOTATOR_H_
