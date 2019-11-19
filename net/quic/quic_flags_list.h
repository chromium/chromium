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

// If true, adjust congestion window when doing bandwidth resumption in BBR.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_bbr_cwnd_in_bandwidth_resumption,
          true)

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

// Enables 3 new connection options to make PROBE_RTT more aggressive
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_less_probe_rtt, false)

// If true, enable QUIC v99.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_99, true)

// When true, set the initial congestion control window from connection options
// in QuicSentPacketManager rather than TcpCubicSenderBytes.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_unified_iw_options, false)

// Number of packets that the pacing sender allows in bursts during pacing.
QUIC_FLAG(int32_t, FLAGS_quic_lumpy_pacing_size, 2)

// Congestion window fraction that the pacing sender allows in bursts during
// pacing.
QUIC_FLAG(double, FLAGS_quic_lumpy_pacing_cwnd_fraction, 0.25f)

// Default enables QUIC ack decimation and adds a connection option to disable
// it.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_ack_decimation, false)

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

// If true, enable experiment for testing PCC congestion-control.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_pcc3, false)

// When true, ensure BBR allows at least one MSS to be sent in response to an
// ACK in packet conservation.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_one_mss_conservation, false)

// Enables the BBQ5 connection option, which forces saved aggregation values to
// expire when the bandwidth increases more than 25% in QUIC BBR STARTUP.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_slower_startup4, true)

// When true and the BBR9 connection option is present, BBR only considers
// bandwidth samples app-limited if they're not filling the pipe.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_flexible_app_limited, false)

// If true, calling StopReading() on a level-triggered QUIC stream sequencer
// will cause the sequencer to discard future data.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_stop_reading_when_level_triggered,
          true)

// When the STMP connection option is sent by the client, timestamps in the QUIC
// ACK frame are sent and processed.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_send_timestamps, false)

// When in STARTUP and recovery, do not add bytes_acked to QUIC BBR's CWND in
// CalculateCongestionWindow()
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_bbr_no_bytes_acked_in_startup_recovery,
    false)

// If true, use common code for checking whether a new stream ID may be
// allocated.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_common_stream_check, false)

// If true, QuicEpollClock::Now() will monotonically increase.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_monotonic_epoll_clock, false)

// If true, enables the BBS4 and BBS5 connection options, which reduce BBR's
// pacing rate in STARTUP as more losses occur as a fraction of CWND.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr_startup_rate_reduction,
          false)

// If true and using Leto for QUIC shared-key calculations, GFE will react to a
// failure to contact Leto by sending a REJ containing a fallback ServerConfig,
// allowing the client to continue the handshake.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_send_quic_fallback_server_config_on_leto_error,
    false)

// If true, enable QUIC version 49.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_49, true)

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

// When true, QUIC Version Negotiation packets will randomly include fake
// versions.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_version_negotiation_grease,
          true)

// If true, use predictable version negotiation versions.
QUIC_FLAG(bool, FLAGS_quic_disable_version_negotiation_grease_randomness, false)

// If true, do not add connection ID of packets with unknown connection ID
// and no version to time wait list, instead, send appropriate responses
// depending on the packets' sizes and drop them.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_reject_unprocessable_packets_statelessly,
    true)

// Maximum number of tracked packets.
QUIC_FLAG(int64_t, FLAGS_quic_max_tracked_packet_count, 10000)

// If true, HTTP request header names sent from QuicSpdyClientBase(and
// descendents) will be automatically converted to lower case.
QUIC_FLAG(bool, FLAGS_quic_client_convert_http_header_name_to_lowercase, true)

// If true, allow client to enable BBRv2 on server via connection option 'B2ON'.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_allow_client_enabled_bbr_v2,
          false)

// If true, will negotiate the ACK delay time.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_negotiate_ack_delay_time, false)

// If true, QuicFramer::WriteClientVersionNegotiationProbePacket uses
// length-prefixed connection IDs.
QUIC_FLAG(bool, FLAGS_quic_prober_uses_length_prefixed_connection_ids, false)

// If true and H2PR connection option is received, write_blocked_streams_ uses
// HTTP2 (tree-style) priority write scheduler.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_use_http2_priority_write_scheduler,
          true)

// If true and FIFO connection option is received, write_blocked_streams uses
// FIFO(stream with smallest ID has highest priority) write scheduler.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_enable_fifo_write_scheduler,
          true)

// If true and LIFO connection option is received, write_blocked_streams uses
// LIFO(stream with largest ID has highest priority) write scheduler.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_enable_lifo_write_scheduler,
          true)

// If true, QuicStreamSequencer will not take in new data if the stream is
// reset.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_no_stream_data_after_reset,
          false)

// If true, enable IETF style probe timeout.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_pto, true)

// The maximum amount of CRYPTO frame data that can be buffered.
QUIC_FLAG(int32_t, FLAGS_quic_max_buffered_crypto_bytes, 16 * 1024)

// If true, QUIC supports both QUIC Crypto and TLS 1.3 for the handshake
// protocol.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_supports_tls_handshake, true)

// If true, enable IETF loss detection as described in
// https://tools.ietf.org/html/draft-ietf-quic-recovery-22#section-6.1.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_enable_ietf_loss_detection,
          true)

// If true, skip packet number before sending the last PTO retransmission.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_skip_packet_number_for_pto,
          true)
// If true, enable HTTP/2 default scheduling(round robin).
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_rr_write_scheduler, true)

// If true, QuicFramer does not create an encrypter/decrypter for the
// ENCRYPTION_INITIAL level.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_framer_doesnt_create_initial_encrypter,
    true)

// If true, treat queued QUIC packets as sent.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_treat_queued_packets_as_sent,
          true)

// Call NeuterHandshakePackets() at most once per connection.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_neuter_handshake_packets_once2,
          false)

// If true, support HTTP/3 priority in v99.
QUIC_FLAG(bool, FLAGS_quic_allow_http3_priority, false)

// If true, enable QUIC version 50.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_version_50, true)

// If true, enable QUIC MTU discovery version 2.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_mtu_discovery_v2, true)

// If true, QUIC connection close packet will be sent at all available
// encryption levels.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_close_all_encryptions_levels,
          false)

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

// If true, Adjacent stream frames will be combined into one stream frame before
// the packet is serialized.
QUIC_FLAG(bool, FLAGS_quic_restart_flag_quic_coalesce_stream_frames_2, false)

// If true, populate nonretransmittable frames in SerializedPacket.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_populate_nonretransmittable_frames,
          true)

// If true, a stream will be reset if it receives fin that has offset less than
// its highest offset.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_no_decrease_in_final_offset,
          false)

// If true, connection will be closed if a stream receives stream frame or
// RESET_STREAM frame with bad close offset.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_close_connection_on_wrong_offset,
          true)

// If true, re-calculate pacing rate when cwnd gets bootstrapped.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr_fix_pacing_rate, true)

// The maximum congestion window in packets.
QUIC_FLAG(int32_t, FLAGS_quic_max_congestion_window, 2000)

// If true, QuicCryptoStream::OnCryptoFrame() will never use the frame's
// encryption level.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_use_connection_encryption_level,
          false)

// If true, do not inject bandwidth in BbrSender::AdjustNetworkParameters.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr_donot_inject_bandwidth,
          false)

// If true, close connection if CreateAndSerializeStreamFrame fails.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_close_connection_on_failed_consume_data_fast_path,
    false)

// If true, add a up call when N packet numbers get skipped.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_on_packet_numbers_skipped,
          false)

// The default minimum duration for BBRv2-native probes, in milliseconds.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_probe_bw_base_duration_ms, 2000)

// The default upper bound of the random amount of BBRv2-native
// probes, in milliseconds.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_probe_bw_max_rand_duration_ms, 1000)

// The default period for entering PROBE_RTT, in milliseconds.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_probe_rtt_period_ms, 10000)

// The default loss threshold for QUIC BBRv2, should be a value
// between 0 and 1.
QUIC_FLAG(double, FLAGS_quic_bbr2_default_loss_threshold, 0.3)

// The default minimum number of loss marking events to exit STARTUP.
QUIC_FLAG(int32_t, FLAGS_quic_bbr2_default_startup_full_loss_count, 8)

// The default fraction of unutilized headroom to try to leave in path
// upon high loss.
QUIC_FLAG(double, FLAGS_quic_bbr2_default_inflight_hi_headroom, 0.01)

// If true, for QUIC BBRv2: 1) don't grow inflight_hi unless it's fully used,
// and 2) cap inflight_lo in PROBE_CRUISE.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr2_fix_inflight_bounds, true)
