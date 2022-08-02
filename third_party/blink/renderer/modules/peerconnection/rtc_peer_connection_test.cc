// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

#include <string>

#include "base/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_answer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_offer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "v8/include/v8.h"

namespace blink {

class RTCOfferOptionsPlatform;

namespace {

static const char* kOfferSdpUnifiedPlanSingleAudioSingleVideo =
    "v=0\r\n"
    "o=- 6676943034916303038 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0 1\r\n"
    "a=msid-semantic: WMS\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 "
    "126\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:pKAt\r\n"
    "a=ice-pwd:bDmIGcCbVl+VkMymNfwdE/Mv\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "F2:D4:95:C5:FC:98:F2:7E:6F:6C:46:BF:5E:05:00:56:4F:A9:BC:4B:1E:56:98:C1:"
    "68:BF:5E:7D:01:A3:EC:93\r\n"
    "a=setup:actpass\r\n"
    "a=mid:0\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=sendrecv\r\n"
    "a=msid:- 36f80301-b634-4c5a-a03b-d1ad79997531\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtcp-fb:111 transport-cc\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:110 telephone-event/48000\r\n"
    "a=rtpmap:112 telephone-event/32000\r\n"
    "a=rtpmap:113 telephone-event/16000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:4264546776 cname:GkUsSfx+DbDplYYT\r\n"
    "a=ssrc:4264546776 msid: 36f80301-b634-4c5a-a03b-d1ad79997531\r\n"
    "a=ssrc:4264546776 mslabel:\r\n"
    "a=ssrc:4264546776 label:36f80301-b634-4c5a-a03b-d1ad79997531\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:pKAt\r\n"
    "a=ice-pwd:bDmIGcCbVl+VkMymNfwdE/Mv\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "F2:D4:95:C5:FC:98:F2:7E:6F:6C:46:BF:5E:05:00:56:4F:A9:BC:4B:1E:56:98:C1:"
    "68:BF:5E:7D:01:A3:EC:93\r\n"
    "a=setup:actpass\r\n"
    "a=mid:1\r\n"
    "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=extmap:4 urn:3gpp:video-orientation\r\n"
    "a=extmap:5 "
    "http://www.ietf.org/id/"
    "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
    "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n"
    "a=extmap:7 "
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\r\n"
    "a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/video-timing\r\n"
    "a=extmap:10 "
    "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07\r\n"
    "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=sendrecv\r\n"
    "a=msid:- 0db71b61-c1ae-4741-bcce-320a254244f3\r\n"
    "a=rtcp-mux\r\n"
    "a=rtcp-rsize\r\n"
    "a=rtpmap:96 VP8/90000\r\n"
    "a=rtcp-fb:96 goog-remb\r\n"
    "a=rtcp-fb:96 transport-cc\r\n"
    "a=rtcp-fb:96 ccm fir\r\n"
    "a=rtcp-fb:96 nack\r\n"
    "a=rtcp-fb:96 nack pli\r\n"
    "a=rtpmap:97 rtx/90000\r\n"
    "a=fmtp:97 apt=96\r\n"
    "a=rtpmap:98 VP9/90000\r\n"
    "a=rtcp-fb:98 goog-remb\r\n"
    "a=rtcp-fb:98 transport-cc\r\n"
    "a=rtcp-fb:98 ccm fir\r\n"
    "a=rtcp-fb:98 nack\r\n"
    "a=rtcp-fb:98 nack pli\r\n"
    "a=fmtp:98 x-google-profile-id=0\r\n"
    "a=rtpmap:99 rtx/90000\r\n"
    "a=fmtp:99 apt=98\r\n"
    "a=rtpmap:100 red/90000\r\n"
    "a=rtpmap:101 rtx/90000\r\n"
    "a=fmtp:101 apt=100\r\n"
    "a=rtpmap:102 ulpfec/90000\r\n"
    "a=ssrc-group:FID 680673332 1566706172\r\n"
    "a=ssrc:680673332 cname:GkUsSfx+DbDplYYT\r\n"
    "a=ssrc:680673332 msid: 0db71b61-c1ae-4741-bcce-320a254244f3\r\n"
    "a=ssrc:680673332 mslabel:\r\n"
    "a=ssrc:680673332 label:0db71b61-c1ae-4741-bcce-320a254244f3\r\n"
    "a=ssrc:1566706172 cname:GkUsSfx+DbDplYYT\r\n"
    "a=ssrc:1566706172 msid: 0db71b61-c1ae-4741-bcce-320a254244f3\r\n"
    "a=ssrc:1566706172 mslabel:\r\n"
    "a=ssrc:1566706172 label:0db71b61-c1ae-4741-bcce-320a254244f3\r\n";

static const char* kOfferSdpUnifiedPlanMultipleAudioTracks =
    "v=0\r\n"
    "o=- 1821816752660535838 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0 1\r\n"
    "a=msid-semantic: WMS\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 "
    "126\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:rbEc\r\n"
    "a=ice-pwd:vmDec3+MrTigDESzNiDuWBnD\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "05:9B:0A:BC:B3:E1:B9:5C:A6:78:96:23:00:0F:96:71:7B:B0:3E:37:87:1D:3A:62:"
    "5E:00:A5:27:22:BB:26:5D\r\n"
    "a=setup:actpass\r\n"
    "a=mid:0\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=sendrecv\r\n"
    "a=msid:- adcd8158-3ad7-4a1f-ac87-8711db959fe8\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtcp-fb:111 transport-cc\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:110 telephone-event/48000\r\n"
    "a=rtpmap:112 telephone-event/32000\r\n"
    "a=rtpmap:113 telephone-event/16000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:2988156579 cname:gr88KGUzymBvrIaJ\r\n"
    "a=ssrc:2988156579 msid: adcd8158-3ad7-4a1f-ac87-8711db959fe8\r\n"
    "a=ssrc:2988156579 mslabel:\r\n"
    "a=ssrc:2988156579 label:adcd8158-3ad7-4a1f-ac87-8711db959fe8\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 "
    "126\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:rbEc\r\n"
    "a=ice-pwd:vmDec3+MrTigDESzNiDuWBnD\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "05:9B:0A:BC:B3:E1:B9:5C:A6:78:96:23:00:0F:96:71:7B:B0:3E:37:87:1D:3A:62:"
    "5E:00:A5:27:22:BB:26:5D\r\n"
    "a=setup:actpass\r\n"
    "a=mid:1\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=sendrecv\r\n"
    "a=msid:- b5f69d2c-e753-4eb5-a302-d41ee75f9fcb\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtcp-fb:111 transport-cc\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:110 telephone-event/48000\r\n"
    "a=rtpmap:112 telephone-event/32000\r\n"
    "a=rtpmap:113 telephone-event/16000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:2562757057 cname:gr88KGUzymBvrIaJ\r\n"
    "a=ssrc:2562757057 msid: b5f69d2c-e753-4eb5-a302-d41ee75f9fcb\r\n"
    "a=ssrc:2562757057 mslabel:\r\n"
    "a=ssrc:2562757057 label:b5f69d2c-e753-4eb5-a302-d41ee75f9fcb\r\n";

static const char* kOfferSdpPlanBSingleAudioSingleVideo =
    "v=0\r\n"
    "o=- 267029810971159627 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE audio video\r\n"
    "a=msid-semantic: WMS 655e92b8-9130-44d8-a188-f5f4633d1a8d "
    "b15218e5-f921-4988-9e1f-6e50ecbd24c2\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 "
    "126\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:ErlQ\r\n"
    "a=ice-pwd:VCnwY8XlD9EX4gpcOHRhU0HV\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "AC:30:90:F9:3B:CB:9A:0D:C6:FB:F3:D6:D6:97:4F:40:A2:B9:5E:4D:F5:32:DC:A7:"
    "B0:3A:33:82:C8:67:FF:7A\r\n"
    "a=setup:actpass\r\n"
    "a=mid:audio\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=sendrecv\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtcp-fb:111 transport-cc\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:110 telephone-event/48000\r\n"
    "a=rtpmap:112 telephone-event/32000\r\n"
    "a=rtpmap:113 telephone-event/16000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:1670492497 cname:rNEKgm1NFupmwR4x\r\n"
    "a=ssrc:1670492497 msid:b15218e5-f921-4988-9e1f-6e50ecbd24c2 "
    "089fd06c-73e4-4720-a6dc-e182eeaeced7\r\n"
    "a=ssrc:1670492497 mslabel:b15218e5-f921-4988-9e1f-6e50ecbd24c2\r\n"
    "a=ssrc:1670492497 label:089fd06c-73e4-4720-a6dc-e182eeaeced7\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:ErlQ\r\n"
    "a=ice-pwd:VCnwY8XlD9EX4gpcOHRhU0HV\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "AC:30:90:F9:3B:CB:9A:0D:C6:FB:F3:D6:D6:97:4F:40:A2:B9:5E:4D:F5:32:DC:A7:"
    "B0:3A:33:82:C8:67:FF:7A\r\n"
    "a=setup:actpass\r\n"
    "a=mid:video\r\n"
    "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=extmap:4 urn:3gpp:video-orientation\r\n"
    "a=extmap:5 "
    "http://www.ietf.org/id/"
    "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
    "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n"
    "a=extmap:7 "
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\r\n"
    "a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/video-timing\r\n"
    "a=extmap:10 "
    "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07\r\n"
    "a=sendrecv\r\n"
    "a=rtcp-mux\r\n"
    "a=rtcp-rsize\r\n"
    "a=rtpmap:96 VP8/90000\r\n"
    "a=rtcp-fb:96 goog-remb\r\n"
    "a=rtcp-fb:96 transport-cc\r\n"
    "a=rtcp-fb:96 ccm fir\r\n"
    "a=rtcp-fb:96 nack\r\n"
    "a=rtcp-fb:96 nack pli\r\n"
    "a=rtpmap:97 rtx/90000\r\n"
    "a=fmtp:97 apt=96\r\n"
    "a=rtpmap:98 VP9/90000\r\n"
    "a=rtcp-fb:98 goog-remb\r\n"
    "a=rtcp-fb:98 transport-cc\r\n"
    "a=rtcp-fb:98 ccm fir\r\n"
    "a=rtcp-fb:98 nack\r\n"
    "a=rtcp-fb:98 nack pli\r\n"
    "a=fmtp:98 x-google-profile-id=0\r\n"
    "a=rtpmap:99 rtx/90000\r\n"
    "a=fmtp:99 apt=98\r\n"
    "a=rtpmap:100 red/90000\r\n"
    "a=rtpmap:101 rtx/90000\r\n"
    "a=fmtp:101 apt=100\r\n"
    "a=rtpmap:102 ulpfec/90000\r\n"
    "a=ssrc-group:FID 3263949794 2166305097\r\n"
    "a=ssrc:3263949794 cname:rNEKgm1NFupmwR4x\r\n"
    "a=ssrc:3263949794 msid:655e92b8-9130-44d8-a188-f5f4633d1a8d "
    "6391e0e8-ac1e-42c2-844c-a7299758db6a\r\n"
    "a=ssrc:3263949794 mslabel:655e92b8-9130-44d8-a188-f5f4633d1a8d\r\n"
    "a=ssrc:3263949794 label:6391e0e8-ac1e-42c2-844c-a7299758db6a\r\n"
    "a=ssrc:2166305097 cname:rNEKgm1NFupmwR4x\r\n"
    "a=ssrc:2166305097 msid:655e92b8-9130-44d8-a188-f5f4633d1a8d "
    "6391e0e8-ac1e-42c2-844c-a7299758db6a\r\n"
    "a=ssrc:2166305097 mslabel:655e92b8-9130-44d8-a188-f5f4633d1a8d\r\n"
    "a=ssrc:2166305097 label:6391e0e8-ac1e-42c2-844c-a7299758db6a\r\n";

static const char* kOfferSdpPlanBMultipleAudioTracks =
    "v=0\r\n"
    "o=- 6228437149521864740 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE audio\r\n"
    "a=msid-semantic: WMS 46f8615e-7599-49f3-9a45-3cf0faf58614 "
    "e01b7c23-2b77-4e09-bee7-4b9140e49647\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 "
    "126\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:Nzla\r\n"
    "a=ice-pwd:PL1APGM2pr773UoUOsj8jzBI\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "DF:8F:89:33:68:AB:55:26:4E:81:CF:95:8C:71:B7:89:45:E7:05:7A:5D:A8:CF:BF:"
    "60:AA:C7:42:F2:85:23:1D\r\n"
    "a=setup:actpass\r\n"
    "a=mid:audio\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=sendrecv\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtcp-fb:111 transport-cc\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:110 telephone-event/48000\r\n"
    "a=rtpmap:112 telephone-event/32000\r\n"
    "a=rtpmap:113 telephone-event/16000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:2716812081 cname:0QgfsHYGSuZjeg5/\r\n"
    "a=ssrc:2716812081 msid:e01b7c23-2b77-4e09-bee7-4b9140e49647 "
    "d73d8a47-3d3f-408f-a2ce-2270eb44ffc5\r\n"
    "a=ssrc:2716812081 mslabel:e01b7c23-2b77-4e09-bee7-4b9140e49647\r\n"
    "a=ssrc:2716812081 label:d73d8a47-3d3f-408f-a2ce-2270eb44ffc5\r\n"
    "a=ssrc:4092260337 cname:0QgfsHYGSuZjeg5/\r\n"
    "a=ssrc:4092260337 msid:46f8615e-7599-49f3-9a45-3cf0faf58614 "
    "6b5f436e-f85d-40a1-83e4-acec63ca4b82\r\n"
    "a=ssrc:4092260337 mslabel:46f8615e-7599-49f3-9a45-3cf0faf58614\r\n"
    "a=ssrc:4092260337 label:6b5f436e-f85d-40a1-83e4-acec63ca4b82\r\n";

RTCSessionDescriptionInit* CreateSdp(String type, String sdp) {
  auto* sdp_init = RTCSessionDescriptionInit::Create();
  sdp_init->setType(type);
  sdp_init->setSdp(sdp);
  return sdp_init;
}

}  // namespace

class RTCPeerConnectionTest : public testing::Test {
 public:
  RTCPeerConnection* CreatePC(
      V8TestingScope& scope,
      const absl::optional<String>& sdp_semantics = absl::nullopt) {
    RTCConfiguration* config = RTCConfiguration::Create();
    if (sdp_semantics)
      config->setSdpSemantics(sdp_semantics.value());
    RTCIceServer* ice_server = RTCIceServer::Create();
    ice_server->setUrl("stun:fake.stun.url");
    HeapVector<Member<RTCIceServer>> ice_servers;
    ice_servers.push_back(ice_server);
    config->setIceServers(ice_servers);
    RTCPeerConnection::SetRtcPeerConnectionHandlerFactoryForTesting(
        base::BindRepeating(
            &RTCPeerConnectionTest::CreateRTCPeerConnectionHandler,
            base::Unretained(this)));
    return RTCPeerConnection::Create(scope.GetExecutionContext(), config,
                                     scope.GetExceptionState());
  }

  virtual std::unique_ptr<RTCPeerConnectionHandler>
  CreateRTCPeerConnectionHandler() {
    return std::make_unique<MockRTCPeerConnectionHandlerPlatform>();
  }

  MediaStreamTrack* CreateTrack(V8TestingScope& scope,
                                MediaStreamSource::StreamType type,
                                String id) {
    auto* source = MakeGarbageCollected<MediaStreamSource>("sourceId", type,
                                                           "sourceName", false);
    auto* component =
        MakeGarbageCollected<MediaStreamComponentImpl>(id, source);
    return MakeGarbageCollected<MediaStreamTrackImpl>(
        scope.GetExecutionContext(), component);
  }

  std::string GetExceptionMessage(V8TestingScope& scope) {
    ExceptionState& exception_state = scope.GetExceptionState();
    return exception_state.HadException() ? exception_state.Message().Utf8()
                                          : "";
  }

  void AddStream(V8TestingScope& scope,
                 RTCPeerConnection* pc,
                 MediaStream* stream) {
    pc->addStream(scope.GetScriptState(), stream, scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

  void RemoveStream(V8TestingScope& scope,
                    RTCPeerConnection* pc,
                    MediaStream* stream) {
    pc->removeStream(stream, scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(RTCPeerConnectionTest, GetAudioTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(pc->GetTrackForTesting(track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrackForTesting(track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetVideoTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeVideo, "videoTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(pc->GetTrackForTesting(track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrackForTesting(track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetAudioAndVideoTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  HeapVector<Member<MediaStreamTrack>> tracks;
  MediaStreamTrack* audio_track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  tracks.push_back(audio_track);
  MediaStreamTrack* video_track =
      CreateTrack(scope, MediaStreamSource::kTypeVideo, "videoTrack");
  tracks.push_back(video_track);

  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(pc->GetTrackForTesting(audio_track->Component()));
  EXPECT_FALSE(pc->GetTrackForTesting(video_track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrackForTesting(audio_track->Component()));
  EXPECT_TRUE(pc->GetTrackForTesting(video_track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCAll) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  MediaStreamComponent* track_component = track->Component();

  {
    HeapVector<Member<MediaStreamTrack>> tracks;
    tracks.push_back(track);
    MediaStream* stream =
        MediaStream::Create(scope.GetExecutionContext(), tracks);
    ASSERT_TRUE(stream);

    EXPECT_FALSE(pc->GetTrackForTesting(track_component));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrackForTesting(track_component));

    RemoveStream(scope, pc, stream);
    // In Unified Plan, transceivers will still reference the stream even after
    // it is "removed". To make the GC tests work, clear the stream from tracks
    // so that the stream does not keep tracks alive.
    while (!stream->getTracks().IsEmpty())
      stream->removeTrack(stream->getTracks()[0], scope.GetExceptionState());
  }

  // This will destroy |MediaStream|, |MediaStreamTrack| and its
  // |MediaStreamComponent|, which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrackForTesting(track_component));
}

TEST_F(RTCPeerConnectionTest,
       GetTrackRemoveStreamAndGCWithPersistentComponent) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  Persistent<MediaStreamComponent> track_component = track->Component();

  {
    HeapVector<Member<MediaStreamTrack>> tracks;
    tracks.push_back(track);
    MediaStream* stream =
        MediaStream::Create(scope.GetExecutionContext(), tracks);
    ASSERT_TRUE(stream);

    EXPECT_FALSE(pc->GetTrackForTesting(track_component.Get()));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrackForTesting(track_component.Get()));

    RemoveStream(scope, pc, stream);
    // In Unified Plan, transceivers will still reference the stream even after
    // it is "removed". To make the GC tests work, clear the stream from tracks
    // so that the stream does not keep tracks alive.
    while (!stream->getTracks().IsEmpty())
      stream->removeTrack(stream->getTracks()[0], scope.GetExceptionState());
  }

  // This will destroy |MediaStream| and |MediaStreamTrack| (but not
  // |MediaStreamComponent|), which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrackForTesting(track_component.Get()));
}

TEST_F(RTCPeerConnectionTest, CheckForComplexSdpWithSdpSemanticsUnifiedPlan) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope, "unified-plan");
  RTCSessionDescriptionInit* sdp = RTCSessionDescriptionInit::Create();
  sdp->setType("offer");
  sdp->setSdp(kOfferSdpUnifiedPlanMultipleAudioTracks);
  ASSERT_TRUE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  ASSERT_EQ(pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)),
            ComplexSdpCategory::kUnifiedPlanExplicitSemantics);
  sdp->setSdp(kOfferSdpPlanBMultipleAudioTracks);
  ASSERT_TRUE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  ASSERT_EQ(pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)),
            ComplexSdpCategory::kPlanBExplicitSemantics);
  sdp->setSdp("invalid sdp");
  ASSERT_TRUE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  ASSERT_EQ(pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)),
            ComplexSdpCategory::kErrorExplicitSemantics);
  // No Complex SDP is detected if only a single track per m= section is used.
  sdp->setSdp(kOfferSdpUnifiedPlanSingleAudioSingleVideo);
  ASSERT_FALSE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  sdp->setSdp(kOfferSdpPlanBSingleAudioSingleVideo);
  ASSERT_FALSE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
}

TEST_F(RTCPeerConnectionTest, CheckForComplexSdpWithSdpSemanticsUnspecified) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  RTCSessionDescriptionInit* sdp = RTCSessionDescriptionInit::Create();
  sdp->setType("offer");
  sdp->setSdp(kOfferSdpPlanBMultipleAudioTracks);
  ASSERT_TRUE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  ASSERT_EQ(pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)),
            ComplexSdpCategory::kPlanBImplicitSemantics);
  sdp->setSdp(kOfferSdpUnifiedPlanMultipleAudioTracks);
  ASSERT_TRUE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  ASSERT_EQ(pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)),
            ComplexSdpCategory::kUnifiedPlanImplicitSemantics);
  sdp->setSdp("invalid sdp");
  ASSERT_TRUE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  ASSERT_EQ(pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)),
            ComplexSdpCategory::kErrorImplicitSemantics);
  // No Complex SDP is detected if only a single track per m= section is used.
  sdp->setSdp(kOfferSdpUnifiedPlanSingleAudioSingleVideo);
  ASSERT_FALSE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
  sdp->setSdp(kOfferSdpPlanBSingleAudioSingleVideo);
  ASSERT_FALSE(
      pc->CheckForComplexSdp(ParsedSessionDescription::Parse(sdp)).has_value());
}

enum class AsyncOperationAction {
  kLeavePending,
  kResolve,
  kReject,
};

template <typename RequestType>
void CompleteRequest(RequestType* request, bool resolve);

template <>
void CompleteRequest(RTCVoidRequest* request, bool resolve) {
  if (resolve) {
    request->RequestSucceeded();
  } else {
    request->RequestFailed(
        webrtc::RTCError(webrtc::RTCErrorType::INVALID_MODIFICATION));
  }
}

template <>
void CompleteRequest(RTCSessionDescriptionRequest* request, bool resolve) {
  if (resolve) {
    auto* description =
        MakeGarbageCollected<RTCSessionDescriptionPlatform>(String(), String());
    request->RequestSucceeded(description);
  } else {
    request->RequestFailed(
        webrtc::RTCError(webrtc::RTCErrorType::INVALID_MODIFICATION));
  }
}

template <typename RequestType>
void PostToCompleteRequest(AsyncOperationAction action, RequestType* request) {
  switch (action) {
    case AsyncOperationAction::kLeavePending:
      return;
    case AsyncOperationAction::kResolve:
      scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
          FROM_HERE,
          base::BindOnce(&CompleteRequest<RequestType>, request, true));
      return;
    case AsyncOperationAction::kReject:
      scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
          FROM_HERE,
          base::BindOnce(&CompleteRequest<RequestType>, request, false));
      return;
  }
}

class FakeRTCPeerConnectionHandlerPlatform
    : public MockRTCPeerConnectionHandlerPlatform {
 public:
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest* request,
      RTCOfferOptionsPlatform*) override {
    PostToCompleteRequest<RTCSessionDescriptionRequest>(async_operation_action_,
                                                        request);
    return {};
  }

  void CreateAnswer(RTCSessionDescriptionRequest* request,
                    RTCAnswerOptionsPlatform*) override {
    PostToCompleteRequest<RTCSessionDescriptionRequest>(async_operation_action_,
                                                        request);
  }

  void SetLocalDescription(RTCVoidRequest* request,
                           ParsedSessionDescription) override {
    PostToCompleteRequest<RTCVoidRequest>(async_operation_action_, request);
  }

  void SetRemoteDescription(RTCVoidRequest* request,
                            ParsedSessionDescription) override {
    PostToCompleteRequest<RTCVoidRequest>(async_operation_action_, request);
  }

  void set_async_operation_action(AsyncOperationAction action) {
    async_operation_action_ = action;
  }

 private:
  // Decides what to do with future async operations' promises/callbacks.
  AsyncOperationAction async_operation_action_ =
      AsyncOperationAction::kLeavePending;
};

TEST_F(RTCPeerConnectionTest, SdpSemanticsUseCounters) {
  // Constructor with default sdpSemantics (= Unified Plan).
  {
    V8TestingScope scope;
    RTCPeerConnection* pc = CreatePC(scope, /*sdp_semantics=*/absl::nullopt);
    // Use counters reflect the constructor's sdpSemantics.
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionConstructedWithPlanB));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionConstructedWithUnifiedPlan));
    // Setting simple Unified Plan SDP does not affect use counters.
    pc->setRemoteDescription(
        scope.GetScriptState(),
        CreateSdp("offer", kOfferSdpUnifiedPlanSingleAudioSingleVideo),
        scope.GetExceptionState());
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexPlanB));
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexUnifiedPlan));
    // Setting complex Unified Plan SDP does affect use counters.
    pc->setRemoteDescription(
        scope.GetScriptState(),
        CreateSdp("offer", kOfferSdpUnifiedPlanMultipleAudioTracks),
        scope.GetExceptionState());
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexPlanB));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexUnifiedPlan));
  }
  // Constructor with {sdpSemantics:"unified-plan"}.
  {
    V8TestingScope scope;
    RTCPeerConnection* pc = CreatePC(scope, "unified-plan");
    // Use counters reflect the constructor's sdpSemantics.
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionConstructedWithPlanB));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionConstructedWithUnifiedPlan));
    // Setting simple Unified Plan SDP does not affect use counters.
    pc->setRemoteDescription(
        scope.GetScriptState(),
        CreateSdp("offer", kOfferSdpUnifiedPlanSingleAudioSingleVideo),
        scope.GetExceptionState());
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexPlanB));
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexUnifiedPlan));
    // Setting complex Unified Plan SDP does affect use counters.
    pc->setRemoteDescription(
        scope.GetScriptState(),
        CreateSdp("offer", kOfferSdpUnifiedPlanMultipleAudioTracks),
        scope.GetExceptionState());
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexPlanB));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kRTCPeerConnectionUsingComplexUnifiedPlan));
  }
}

TEST_F(RTCPeerConnectionTest, MediaStreamTrackStopsThrottling) {
  V8TestingScope scope;

  auto* scheduler = scope.GetFrame().GetFrameScheduler()->GetPageScheduler();
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Creating the RTCPeerConnection doesn't disable throttling.
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // But creating a media stream track does.
  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);
  EXPECT_TRUE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Stopping the track disables the opt-out.
  track->stopTrack(scope.GetExecutionContext());
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());
}

}  // namespace blink
