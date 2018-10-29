// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_rtp_sender.h"
#include "third_party/blink/public/platform/web_rtc_session_description.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_configuration.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_server.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_session_description_init.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_web_rtc.h"

namespace blink {

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

class RTCPeerConnectionTest : public testing::Test {
 public:
  RTCPeerConnection* CreatePC(V8TestingScope& scope,
                              const String& sdpSemantics = String()) {
    RTCConfiguration config;
    config.setSdpSemantics(sdpSemantics);
    RTCIceServer ice_server;
    ice_server.setURL("stun:fake.stun.url");
    HeapVector<RTCIceServer> ice_servers;
    ice_servers.push_back(ice_server);
    config.setIceServers(ice_servers);
    return RTCPeerConnection::Create(scope.GetExecutionContext(), config,
                                     Dictionary(), scope.GetExceptionState());
  }

  MediaStreamTrack* CreateTrack(V8TestingScope& scope,
                                MediaStreamSource::StreamType type,
                                String id) {
    MediaStreamSource* source =
        MediaStreamSource::Create("sourceId", type, "sourceName", false);
    MediaStreamComponent* component = MediaStreamComponent::Create(id, source);
    return MediaStreamTrack::Create(scope.GetExecutionContext(), component);
  }

  std::string GetExceptionMessage(V8TestingScope& scope) {
    ExceptionState& exception_state = scope.GetExceptionState();
    return exception_state.HadException()
               ? exception_state.Message().Utf8().data()
               : "";
  }

  void AddStream(V8TestingScope& scope,
                 RTCPeerConnection* pc,
                 MediaStream* stream) {
    pc->addStream(scope.GetScriptState(), stream, Dictionary(),
                  scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

  void RemoveStream(V8TestingScope& scope,
                    RTCPeerConnection* pc,
                    MediaStream* stream) {
    pc->removeStream(stream, scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithWebRTC> platform;
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

  EXPECT_FALSE(pc->GetTrack(track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrack(track->Component()));
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

  EXPECT_FALSE(pc->GetTrack(track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrack(track->Component()));
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

  EXPECT_FALSE(pc->GetTrack(audio_track->Component()));
  EXPECT_FALSE(pc->GetTrack(video_track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrack(audio_track->Component()));
  EXPECT_TRUE(pc->GetTrack(video_track->Component()));
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

    EXPECT_FALSE(pc->GetTrack(track_component));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrack(track_component));

    RemoveStream(scope, pc, stream);
  }

  // This will destroy |MediaStream|, |MediaStreamTrack| and its
  // |MediaStreamComponent|, which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrack(track_component));
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

    EXPECT_FALSE(pc->GetTrack(track_component.Get()));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrack(track_component.Get()));

    RemoveStream(scope, pc, stream);
  }

  // This will destroy |MediaStream| and |MediaStreamTrack| (but not
  // |MediaStreamComponent|), which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrack(track_component.Get()));
}

TEST_F(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCWithPersistentStream) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  MediaStreamComponent* track_component = track->Component();
  Persistent<MediaStream> stream;

  {
    HeapVector<Member<MediaStreamTrack>> tracks;
    tracks.push_back(track);
    stream = MediaStream::Create(scope.GetExecutionContext(), tracks);
    ASSERT_TRUE(stream);

    EXPECT_FALSE(pc->GetTrack(track_component));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrack(track_component));

    RemoveStream(scope, pc, stream);
  }

  // With a persistent |MediaStream|, the |MediaStreamTrack| and
  // |MediaStreamComponent| will not be destroyed and continue to be mapped by
  // peer connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_TRUE(pc->GetTrack(track_component));

  stream = nullptr;
  // Now |MediaStream|, |MediaStreamTrack| and |MediaStreamComponent| will be
  // destroyed and the mapping removed from the peer connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrack(track_component));
}

TEST_F(RTCPeerConnectionTest, PlanBSdpWarningNotShownWhenPlanBSpecified) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope, "plan-b");
  RTCSessionDescriptionInit sdp;
  sdp.setType("offer");
  // It doesn't matter the SDP, never show a warning if sdpSemantics was
  // specified at construction.
  sdp.setSdp(kOfferSdpUnifiedPlanSingleAudioSingleVideo);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpUnifiedPlanMultipleAudioTracks);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpPlanBSingleAudioSingleVideo);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpPlanBMultipleAudioTracks);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
}

TEST_F(RTCPeerConnectionTest, PlanBSdpWarningNotShownWhenUnifiedPlanSpecified) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope, "unified-plan");
  RTCSessionDescriptionInit sdp;
  sdp.setType("offer");
  // It doesn't matter the SDP, never show a warning if sdpSemantics was
  // specified at construction.
  sdp.setSdp(kOfferSdpUnifiedPlanSingleAudioSingleVideo);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpUnifiedPlanMultipleAudioTracks);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpPlanBSingleAudioSingleVideo);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpPlanBMultipleAudioTracks);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
}

TEST_F(RTCPeerConnectionTest, PlanBSdpWarningNotShownWhenInvalidSdp) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  RTCSessionDescriptionInit sdp;
  sdp.setType("offer");
  sdp.setSdp("invalid sdp");
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
}

TEST_F(RTCPeerConnectionTest, PlanBSdpWarningNotShownForSingleTracks) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  RTCSessionDescriptionInit sdp;
  sdp.setType("offer");
  // Neither Unified Plan or Plan B SDP should result in a warning if only a
  // single track per m= section is used.
  sdp.setSdp(kOfferSdpUnifiedPlanSingleAudioSingleVideo);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
  sdp.setSdp(kOfferSdpPlanBSingleAudioSingleVideo);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
}

TEST_F(RTCPeerConnectionTest, PlanBSdpWarningShownForComplexPlanB) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  RTCSessionDescriptionInit sdp;
  sdp.setType("offer");
  sdp.setSdp(kOfferSdpPlanBMultipleAudioTracks);
  ASSERT_TRUE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
}

TEST_F(RTCPeerConnectionTest, PlanBSdpWarningNotShownForComplexUnifiedPlan) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  RTCSessionDescriptionInit sdp;
  sdp.setType("offer");
  sdp.setSdp(kOfferSdpUnifiedPlanMultipleAudioTracks);
  ASSERT_FALSE(pc->ShouldShowComplexPlanBSdpWarning(sdp));
}

}  // namespace blink
