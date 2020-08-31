// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate values. The following line silences a
// presubmit warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

// This file contains the list of QUIC protocol flags.

// Time period for which a given connection_id should live in the time-wait
// state.
QUIC_FLAG(int64_t, FLAGS_quic_time_wait_list_seconds, 200)

// Currently, this number is quite conservative.  The max QPS limit for an
// individual server silo is currently set to 1000 qps, though the actual max
// that we see in the wild is closer to 450 qps.  Regardless, this means that
// the longest time-wait list we should see is 200 seconds * 1000 qps, 200000.
// Of course, there are usually many queries per QUIC connection, so we allow a
// factor of 3 leeway.
//
// Maximum number of connections on the time-wait list. A negative value implies
// no configured limit.
QUIC_FLAG(int64_t, FLAGS_quic_time_wait_list_max_connections, 600000)

// If true, require handshake confirmation for QUIC connections, functionally
// disabling 0-rtt handshakes.
// TODO(rtenneti): Enable this flag after CryptoServerTest's are fixed.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_require_handshake_confirmation,
          false)

// If true, disable pacing in QUIC.
QUIC_FLAG(bool, FLAGS_quic_disable_pacing_for_perf_tests, false)

// If true, enforce that QUIC CHLOs fit in one packet.
QUIC_FLAG(bool, FLAGS_quic_enforce_single_packet_chlo, true)

// If true, allows packets to be buffered in anticipation of a future CHLO, and
// allow CHLO packets to be buffered until next iteration of the event loop.
QUIC_FLAG(bool, FLAGS_quic_allow_chlo_buffering, true)

// If greater than zero, mean RTT variation is multiplied by the specified
// factor and added to the congestion window limit.
QUIC_FLAG(double, FLAGS_quic_bbr_rtt_variation_weight, 0.0f)

// Congestion window gain for QUIC BBR during PROBE_BW phase.
QUIC_FLAG(double, FLAGS_quic_bbr_cwnd_gain, 2.0f)

// When true, defaults to BBR congestion control instead of Cubic.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_default_to_bbr, false)

// If true, use BBRv2 as the default congestion controller.
// Takes precedence over --quic_default_to_bbr.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_default_to_bbr_v2, false)

// If buffered data in QUIC stream is less than this threshold, buffers all
// provided data or asks upper layer for more data.
QUIC_FLAG(uint32_t, FLAGS_quic_buffered_data_threshold, 8192u)

// Max size of data slice in bytes for QUIC stream send buffer.
QUIC_FLAG(uint32_t, FLAGS_quic_send_buffer_max_data_slice_size, 4096u)

// Anti-amplification factor. Before address validation, server will
// send no more than factor times bytes received.
QUIC_FLAG(int32_t, FLAGS_quic_anti_amplification_factor, 3)

// When true, set the initial congestion control window from connection options
// in QuicSentPacketManager rather than TcpCubicSenderBytes.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_unified_iw_options, false)

// Number of packets that the pacing sender allows in bursts during pacing.
QUIC_FLAG(int32_t, FLAGS_quic_lumpy_pacing_size, 2)

// Congestion window fraction that the pacing sender allows in bursts during
// pacing.
QUIC_FLAG(double, FLAGS_quic_lumpy_pacing_cwnd_fraction, 0.25f)

// If true, QUIC offload pacing when using USPS as egress method.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_offload_pacing_to_usps2, false)

// Max time that QUIC can pace packets into the future in ms.
QUIC_FLAG(int32_t, FLAGS_quic_max_pace_time_into_future_ms, 10)

// Smoothed RTT fraction that a connection can pace packets into the future.
QUIC_FLAG(double, FLAGS_quic_pace_time_into_future_srtt_fraction, 0.125f)

// Mechanism to override version label and ALPN for IETF interop.
QUIC_FLAG(int32_t, FLAGS_quic_ietf_draft_version, 0)

// If true, stop resetting ideal_next_packet_send_time_ in pacing sender.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_donot_reset_ideal_next_packet_send_time,
    false)

// When true and the BBR9 connection option is present, BBR only considers
// bandwidth samples app-limited if they're not filling the pipe.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_flexible_app_limited, false)

// When the STMP connection option is sent by the client, timestamps in the QUIC
// ACK frame are sent and processed.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_send_timestamps, false)

// When in STARTUP and recovery, do not add bytes_acked to QUIC BBR's CWND in
// CalculateCongestionWindow()
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_bbr_no_bytes_acked_in_startup_recovery,
    false)

// If true and using Leto for QUIC shared-key calculations, GFE will react to a
// failure to contact Leto by sending a REJ containing a fallback ServerConfig,
// allowing the client to continue the handshake.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_send_quic_fallback_server_config_on_leto_error,
    false)

// If true, GFE will not request private keys when fetching QUIC ServerConfigs
// from Leto.
QUIC_FLAG(bool,
          FLAGS_quic_restart_flag_dont_fetch_quic_private_keys_from_leto,
          false)

// In v44 and above, where STOP_WAITING is never sent, close the connection if
// it's received.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_do_not_accept_stop_waiting,
          false)

// If true, set burst token to 2 in cwnd bootstrapping experiment.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_conservative_bursts, false)

// If true, export number of packets written per write operation histogram.")
QUIC_FLAG(bool, FLAGS_quic_export_server_num_packets_per_write_histogram, false)

// If true, uses conservative cwnd gain and pacing gain.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_conservative_cwnd_and_pacing_gains,
          false)

// If true, use predictable version negotiation versions.
QUIC_FLAG(bool, FLAGS_quic_disable_version_negotiation_grease_randomness, false)

// Maximum number of tracked packets.
QUIC_FLAG(int64_t, FLAGS_quic_max_tracked_packet_count, 10000)

// If true, HTTP request header names sent from QuicSpdyClientBase(and
// descendents) will be automatically converted to lower case.
QUIC_FLAG(bool, FLAGS_quic_client_convert_http_header_name_to_lowercase, true)

// If true, allow client to enable BBRv2 on server via connection option 'B2ON'.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_allow_client_enabled_bbr_v2,
          false)

// If true, QuicFramer::WriteClientVersionNegotiationProbePacket uses
// length-prefixed connection IDs.
QUIC_FLAG(bool, FLAGS_quic_prober_uses_length_prefixed_connection_ids, false)

// The maximum amount of CRYPTO frame data that can be buffered.
QUIC_FLAG(int32_t, FLAGS_quic_max_buffered_crypto_bytes, 16 * 1024)

// If the bandwidth during ack aggregation is smaller than (estimated
// bandwidth * this flag), consider the current aggregation completed
// and starts a new one.
QUIC_FLAG(double, FLAGS_quic_ack_aggregation_bandwidth_threshold, 1.0)

// If set to non-zero, the maximum number of consecutive pings that can be sent
// with aggressive initial retransmittable on wire timeout if there is no new
// data received. After which, the timeout will be exponentially back off until
// exceeds the default ping timeout.
QUIC_FLAG(int32_t,
          FLAGS_quic_max_aggressive_retransmittable_on_wire_ping_count,
          0)

// The maximum congestion window in packets.
QUIC_FLAG(int32_t, FLAGS_quic_max_congestion_window, 2000)

// The default minimum duration for BBRv2-native probes, in milliseconds.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_probe_bw_base_duration_ms, 2000)

// The default upper bound of the random amount of BBRv2-native
// probes, in milliseconds.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_probe_bw_max_rand_duration_ms, 1000)

// The default period for entering PROBE_RTT, in milliseconds.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_probe_rtt_period_ms, 10000)

// The default loss threshold for QUIC BBRv2, should be a value
// between 0 and 1.
QUIC_FLAG(double, FLAGS_quic_bbr2_default_loss_threshold, 0.02)

// The default minimum number of loss marking events to exit STARTUP.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_startup_full_loss_count, 8)

// The default fraction of unutilized headroom to try to leave in path
// upon high loss.
QUIC_FLAG(double, FLAGS_quic_bbr2_default_inflight_hi_headroom, 0.01)

// If true, disable QUIC version Q043.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_q043, false)

// If true, disable QUIC version Q046.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_q046, false)

// If true, disable QUIC version Q050.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_q050, false)

// A testonly reloadable flag that will always default to false.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_testonly_default_false, false)

// A testonly reloadable flag that will always default to true.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_testonly_default_true, true)

// A testonly restart flag that will always default to false.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_testonly_default_false, false)

// A testonly restart flag that will always default to true.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_testonly_default_true, true)

// The default initial value of the max ack height filter's window length.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_initial_ack_height_filter_window, 10)

// The default minimum number of loss marking events to exit PROBE_UP phase.
QUIC_FLAG(double, FLAGS_quic_bbr2_default_probe_bw_full_loss_count, 2)

// When true, ensure the ACK delay is never less than the alarm granularity when
// ACK decimation is enabled.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_ack_delay_alarm_granularity,
          false)

// If true, use predictable grease settings identifiers and values.
QUIC_FLAG(bool, FLAGS_quic_enable_http3_grease_randomness, true)

// If true, disable QUIC version h3-27.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_draft_27, false)

// If true, server push will be allowed in QUIC versions using HTTP/3.
QUIC_FLAG(bool, FLAGS_quic_enable_http3_server_push, false)

// The divisor that controls how often MAX_STREAMS frames are sent.
QUIC_FLAG(int32_t, FLAGS_quic_max_streams_window_divisor, 2)

// If true, QUIC BBRv2\'s PROBE_BW mode will not reduce cwnd below
// BDP+ack_height.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr2_avoid_too_low_probe_bw_cwnd,
          false)

// When true, the 1RTT and 2RTT connection options decrease the number of round
// trips in BBRv2 STARTUP without a 25% bandwidth increase to 1 or 2 round trips
// respectively.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr2_fewer_startup_round_trips,
          false)

// If true, enables support for TLS resumption in QUIC.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_enable_tls_resumption_v4, true)

// When true, QUIC's BBRv2 ignores inflight_lo in PROBE_BW.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr2_ignore_inflight_lo, true)

// If true, support for IETF QUIC 0-rtt is enabled.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_enable_zero_rtt_for_tls_v2, true)

// If true, default on PTO which unifies TLP + RTO loss recovery.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_default_on_pto, false)

// If true, the B2HI connection option limits reduction of inflight_hi to
// (1-Beta)*CWND.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr2_limit_inflight_hi, false)

// If true, disable QUIC version h3-T050.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_t050, false)

// If true, default-enable 5RTO blachole detection.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_default_enable_5rto_blackhole_detection2,
    true)

// If true, disable QUIC version h3-29.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_draft_29, false)

// When true, QuicDispatcher supports decapsulation of Legacy Version
// Encapsulation packets.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_dispatcher_legacy_version_encapsulation,
    true)

// If true, update packet size when the first frame gets queued.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_update_packet_size, true)

// If true, consider frame expansion when calculating extra padding bytes to
// meet minimum plaintext packet size required for header protection.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_extra_padding_bytes, true)

// If true, do not try to close stream again if stream fails to be closed upon
// connection close.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_do_not_close_stream_again_on_connection_close,
    true)

// If true, determine a serialized packet's fate before the packet gets
// serialized.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_determine_serialized_packet_fate_early,
    true)

// If true, take the largest acked packet into account when computing the sent
// packet number length.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_packet_number_length, true)

// If true, retransmit unacked handshake data before PTO expiry.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_retransmit_handshake_data_early,
          true)

// If true, improve Bbr2Sender::AdjustNetworkParameters by 1) do not inject a
// bandwidth sample to the bandwidth filter, and 2) re-calculate pacing rate
// after cwnd updated..
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_bbr2_improve_adjust_network_parameters,
    true)

// If true, try to coalesce packet of higher space with retransmissions to
// mitigate RTT inflations.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_coalesced_packet_of_higher_space2,
          true)

// If true, record the received min_ack_delay in transport parameters to QUIC
// config.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_record_received_min_ack_delay,
          false)

// If true, QuicSession will no longer need streams_waiting_for_acks_.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_remove_streams_waiting_for_acks,
          true)

// When true, ParsedQuicVersionToString will print IETF drafts with format
// draft29 instead of ff00001d.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_print_draft_version, true)

// If true, disable blackhole detection on server side.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_disable_server_blackhole_detection,
          false)

// Remove ACK_DECIMATION_WITH_REORDERING mode and fast_ack_after_quiescence
// option in QUIC received packet manager.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_remove_unused_ack_options, true)

// If true, QUIC subclasses will no longer directly access stream_map for its
// content.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_do_not_use_stream_map, true)

// If true,
//   server accepts GOAWAY (draft-28 behavior),
//   client receiving GOAWAY with stream ID that is not client-initiated
//     bidirectional stream ID closes connection with H3_ID_ERROR (draft-28
//     behavior).
//   Also, receiving a GOAWAY with ID larger than previously received closes
//     connection with H3_ID_ERROR.
// If false,
//   server receiving GOAWAY closes connection with H3_FRAME_UNEXPECTED
//     (draft-27 behavior),
//   client receiving GOAWAY with stream ID that is not client-initiated
//     bidirectional stream ID closes connection with PROTOCOL_VIOLATION
//     (draft-04 behavior),
//   larger ID than previously received does not trigger connection close.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_http3_goaway_new_behavior, true)

// If true, QUIC connection will revert to a previously validated MTU (if
// exists) after two PTOs.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_revert_mtu_after_two_ptos, true)

// Simplify the ACK code in quic_received_packet_manager.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_simplify_received_packet_manager_ack,
          true)

// If true, when TLPR copt is used, enable half RTT as first PTO timeout.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_half_rtt_as_first_pto, true)

// If true, enable overshooting detection when the DTOS connection option is
// supplied.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_enable_overshooting_detection,
          true)

// If true, enable QUIC version h3-T051.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_t051, true)

// If true, fix a case where data is marked lost in HANDSHAKE level but
// HANDSHAKE key gets decrypted later.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_neuter_handshake_data, true)

// If true, when data is sending in fast path mode in the creator, making sure
// stream data is sent in the right encryption level.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_check_encryption_level_in_fast_path,
          true)

// If true, gQUIC will only consult stream_map in
// QuicSession::GetNumActiveStreams().
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_get_stream_information_from_stream_map,
    true)

// If true, QuicSession does not keep a separate zombie_streams. Instead, all
// streams are stored in stream_map_.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_remove_zombie_streams, false)

// If true, always send connection close for idle timeout if NSLC is received.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_no_silent_close_for_idle_timeout,
          false)

// If true, when server is silently closing connections due to idle timeout,
// serialize the connection close packets which will be added to time wait list.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_add_silent_idle_timeout, false)

// If true, do not send PING if ShouldKeepConnectionAlive is false.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_on_ping_timeout, true)

// When true, QUIC+TLS versions will send the key_update_not_yet_supported
// transport parameter.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_send_key_update_not_yet_supported,
          true)

// If true, QUIC will default enable MTU discovery, with a target of 1450 bytes.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_enable_mtu_discovery_at_server,
          false)

// If true, neuter initial packet in the coalescer when discarding initial keys.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_neuter_initial_packet_in_coalescer_with_initial_key_discarded,
    true)
