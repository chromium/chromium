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

// When the STMP connection option is sent by the client, timestamps in the QUIC
// ACK frame are sent and processed.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_send_timestamps, false)

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

// The maximum amount of CRYPTO frame data that can be buffered.
QUIC_FLAG(int32_t, FLAGS_quic_max_buffered_crypto_bytes, 16 * 1024)

// If the bandwidth during ack aggregation is smaller than (estimated
// bandwidth * this flag), consider the current aggregation completed
// and starts a new one.
QUIC_FLAG(double, FLAGS_quic_ack_aggregation_bandwidth_threshold, 1.0)

// Maximum number of consecutive pings that can be sent with the aggressive
// initial retransmittable on the wire timeout if there is no new stream data
// received. After this limit, the timeout will be doubled each ping until it
// exceeds the default ping timeout.
QUIC_FLAG(int32_t,
          FLAGS_quic_max_aggressive_retransmittable_on_wire_ping_count,
          5)

// Maximum number of pings that can be sent with the retransmittable on the wire
// timeout, over the lifetime of a connection. After this limit, the timeout
// will be the default ping timeout.
QUIC_FLAG(int32_t, FLAGS_quic_max_retransmittable_on_wire_ping_count, 1000)

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
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_draft_27, true)

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

// If true, record the received min_ack_delay in transport parameters to QUIC
// config.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_record_received_min_ack_delay,
          false)

// If true, disable blackhole detection on server side.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_disable_server_blackhole_detection,
          false)

// If true, QUIC will default enable MTU discovery, with a target of 1450 bytes.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_enable_mtu_discovery_at_server,
          false)

// If true, while reading an IETF quic packet, start peer migration immediately
// when detecting the existence of any non-probing frame instead of at the end
// of the packet.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_start_peer_migration_earlier,
          false)

// If true, neuter initial packet in the coalescer when discarding initial keys.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_neuter_initial_packet_in_coalescer_with_initial_key_discarded,
    true)

// If true, convert bytes_left_for_batch_write_ to unsigned int.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_bytes_left_for_batch_write,
          true)

// If true, add missing connected checks.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_add_missing_connected_checks,
          true)

// If true, QuicStream::kDefaultUrgency is 3, otherwise 1.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_http3_new_default_urgency_value,
          true)

// If true, close connection on packet serialization failures
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_close_connection_on_serialization_failure,
    true)

// If true, send PATH_RESPONSE upon receiving PATH_CHALLENGE regardless
// of perspective.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_send_path_response, false)

// If true, when switching from BBR to BBR2, use BBR's CWND as the initial CWND.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_copy_bbr_cwnd_to_bbr2, true)

// If true, send the lowest stream ID that can be retried by the client in a
// GOAWAY frame.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_http3_goaway_stream_id,
          true)

// If true, close connection if writer is still blocked when OnCanWrite is
// called.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_close_connection_in_on_can_write_with_blocked_writer,
    true)

// If true, include stream information in idle timeout connection close detail.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_add_stream_info_to_idle_close_detail,
          true)

// If true, use IETF QUIC application error codes in STOP_SENDING frames.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_stop_sending_uses_ietf_error_code,
          true)

// If true, QuicSpdySession's destructor won't need to do cleanup.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_clean_up_spdy_session_destructor,
          true)

// If true, discard INITIAL packet if the key has been dropped.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_discard_initial_packet_with_key_dropped,
    true)

// If true, disable QUIC version h3-T051.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_disable_version_t051, false)

// If true, make sure there is pending timer credit when trying to PTO
// retransmit any packets.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_pto_pending_timer_count,
          true)

// If true, QUIC connection will pass sent packet information to the debug
// visitor after a packet is recorded as sent in sent packet manager.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_give_sent_packet_to_debug_visitor_after_sent,
    true)

// If true, abort async QPACK header decompression in QuicSpdyStream::OnClose().
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_abort_qpack_on_stream_close,
          true)

// If true, do not arm PTO for application data until handshake confirmed.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_arm_pto_for_application_data,
          true)

// If true, cap client suggested initial RTT to 1s if it is longer than 1s.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_cap_large_client_initial_rtt,
          true)

// If true, fix a potential out of order sending caused by handshake gets
// confirmed while the coalescer is not empty.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_out_of_order_sending2, true)

// If true, remove processed undecryptable packets.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_fix_undecryptable_packets2,
          true)

// If true, QUIC BBRv2 will use inflight byte after congestion event to detect
// queuing during PROBE_UP.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_bbr2_use_post_inflight_to_detect_queuing,
    true)

// If true, QUIC BBRv2 will use 15% inflight_hi headroom, which is the default
// for TCP.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr2_use_tcp_inflight_hi_headroom,
          true)

// If true, HTTP/3 will treat HTTP/2 specific SETTINGS as error.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_reject_spdy_settings, false)

// If true, discard 0-RTT keys after installing 1-RTT keys on the client side.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_postpone_discarding_zero_rtt_keys,
          true)

// If true, for IETF QUIC, uses 2 * RTTVAR when calculating PTO delay.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_default_to_2_rttvar, true)

// Deallocate data in QuicMessageFrame right after the corresponding packet is
// sent.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_deallocate_message_right_after_sent,
          true)

// If true, drop initial keys at the end of writing and unify the fixes for
// missing initial keys.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_missing_initial_keys2, true)

// If true, check whether framer has the right key before writing data.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_check_keys_before_writing, true)

// If true, received error codes larger than QUIC_LAST_ERROR are preserved.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_do_not_clip_received_error_code,
          false)

// If true, HTTP/3 sesions will report error and close connection upon receiving
// HTTP/2 only frames.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_reject_spdy_frames, false)

// If true, QuicConnection will initialize its self address to the self address
// of the first received packet, for all server connections and client
// connections that know its own address.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_connection_set_initial_self_address,
          false)

// If true, let QUIC connection handle PINGs instead of going through session.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_let_connection_handle_pings,
          true)

// If true, BBRv2 will 1) change the default STARTUP and DRAIN cwnd gain to 2.0,
// and 2) change the meaning of connection option BBQ2 to use 2.885 for STARTUP
// and DRAIN cwnd gain.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_bbr2_flip_bbq2, true)

// If true, use http2::HuffmanEncodeFast() instead of HuffmanEncode() and
// eliminate one string copy for QPACK encoding used in IETF QUIC.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_use_fast_huffman_encoder, false)

// When true, QUIC+TLS versions will support key updates.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_key_update_supported, false)

// If true, address is validated by successfully processing a HANDSHAKE or 1-RTT
// packet.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_fix_address_validation, true)

// If true, QuicStream will explicitly specify which RST_STREAM, STOP_SENDING
// frame to send.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_split_up_send_rst, false)

// If true, send HTTP/3 GOAWAY frame when sending CONNECTION_CLOSE.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_send_goaway_with_connection_close,
          false)

// If true, ack frequency frame can be sent from server to client.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_can_send_ack_frequency, false)

// If true, QUIC BBRv2 will support NetworkParams.max_initial_congestion_window
// when bootstrapping cwnd.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_bbr2_support_max_bootstrap_cwnd,
          false)

// If true, QUIC BBR2 will not exit STARTUP on excessive loss, if there was
// enough bandwidth growth in round.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_bbr2_no_exit_startup_on_loss_with_bw_growth,
    false)

// If non-zero and key update is allowed, the maximum number of packets sent for
// each key phase before initiating a key update.
QUIC_FLAG(int64_t, FLAGS_quic_key_update_confidentiality_limit, 0)

// Honor the AEAD confidentiality and integrity limits by initiating key update
// (if enabled) and/or closing the connection, as necessary.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_enable_aead_limits, false)

// If true, try to process undecryptable packets at the end of
// TlsServerHandshaker::DecryptCallback, if it is callback is run
// asynchronously.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_process_undecryptable_packets_after_async_decrypt_callback,
    false)

// When true, QUIC server will send version negotiation packets even if the
// original connection ID was under 64bits in length.
QUIC_FLAG(
    bool,
    FLAGS_quic_reloadable_flag_quic_send_version_negotiation_for_short_connection_ids,
    false)

// If true, use one of the 12 QPACK encoder stream error codes and 5 QPACK
// decoder stream error codes and QUIC_INTERNAL_ERROR instead of the two generic
// ones.
QUIC_FLAG(bool,
          FLAGS_quic_reloadable_flag_quic_granular_qpack_error_codes,
          false)

// When true, the server delays its Initial ACK-only packets the full
// max_ack_delay.
QUIC_FLAG(bool, FLAGS_quic_reloadable_flag_quic_delay_initial_ack, false)

// If true, session tickets will always be enabled in QUIC.
QUIC_FLAG(bool,
          FLAGS_quic_restart_flag_quic_session_tickets_always_enabled,
          false)

// If true, QUIC client with TLS will not try 0-RTT.
QUIC_FLAG(bool, FLAGS_quic_disable_client_tls_zero_rtt, false)
