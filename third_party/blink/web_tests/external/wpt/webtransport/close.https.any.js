// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=resources/webtransport-test-helpers.sub.js
// META: script=/common/utils.js

promise_test(async t => {
  const id = token();
  let wt = new WebTransport(webtransport_url(`client-close.py?token=${id}`));
  await wt.ready;

  wt.close();

  const close_info = await wt.closed;

  assert_not_own_property(close_info, 'code');
  assert_not_own_property(close_info, 'reason');

  wt = new WebTransport(webtransport_url(`query.py?token=${id}`));
  await wt.ready;

  const streams = await wt.incomingUnidirectionalStreams;
  const streams_reader = streams.getReader();
  const { value: readable } = await streams_reader.read();
  streams_reader.releaseLock();

  const data = await read_stream_as_json(readable);

  assert_own_property(data, 'session-close-info');
  const info = data['session-close-info']

  assert_false(info.abruptly, 'abruptly');
  assert_equals(info.close_info, null, 'close_info');
}, 'close');

promise_test(async t => {
  const id = token();
  let wt = new WebTransport(webtransport_url(`client-close.py?token=${id}`));
  await wt.ready;

  wt.close({code: 99, reason: 'reason'});

  const close_info = await wt.closed;

  assert_equals(close_info.code, 99, 'code');
  assert_equals(close_info.reason, 'reason X', 'reason');

  wt = new WebTransport(webtransport_url(`query.py?token=${id}`));
  await wt.ready;

  const streams = await wt.incomingUnidirectionalStreams;
  const streams_reader = streams.getReader();
  const { value: readable } = await streams_reader.read();
  streams_reader.releaseLock();

  const data = await read_stream_as_json(readable);

  assert_own_property(data, 'session-close-info');
  const info = data['session-close-info']

  assert_false(info.abruptly, 'abruptly');
  assert_equals(info.close_info.code, 99, 'code');
  assert_equals(info.close_info.reason, 'reason X', 'reason');
}, 'close with code and reason');

promise_test(async t => {
  const id = token();
  let wt = new WebTransport(webtransport_url(`client-close.py?token=${id}`));
  await wt.ready;
  const reason = 'あいうえお'.repeat(1000);

  wt.close({code: 11, reason});

  const close_info = await wt.closed;

  assert_equals(close_info.code, 11, 'code');
  assert_equals(close_info.reason, reason, 'reason');

  wt = new WebTransport(webtransport_url(`query.py?token=${id}`));
  await wt.ready;

  const streams = await wt.incomingUnidirectionalStreams;
  const streams_reader = streams.getReader();
  const { value: readable } = await streams_reader.read();
  streams_reader.releaseLock();

  const data = await read_stream_as_json(readable);

  assert_own_property(data, 'session-close-info');
  const info = data['session-close-info']

  const expectedReason =
    new TextDecoder().decode(new TextEncoder().encode(reason).slice(1024))
  assert_false(info.abruptly, 'abruptly');
  assert_equals(info.close_info.code, 11, 'code');
  assert_equals(info.close_info.reason, expectedReason, 'reason');
}, 'close with code and long reason');