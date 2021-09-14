import pytest


@pytest.mark.asyncio
@pytest.mark.parametrize("log_argument, expected_text", [
    ("'TEST'", "TEST"),
    ("'TWO', 'PARAMETERS'", "TWO PARAMETERS"),
    ("{}", "[object Object]"),
    ("['1', '2', '3']", "1,2,3"),
    ("null, undefined", "null undefined"),
], ids=[
    'single string',
    'two strings',
    'empty object',
    'array of strings',
    'null and undefined',
])
async def test_console_log_argument_type(bidi_session,
                                         current_session,
                                         wait_for_event,
                                         log_argument,
                                         expected_text):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    # TODO: To be replaced with the BiDi implementation of execute_script.
    current_session.execute_script(f"console.log({log_argument})")

    event_data = await on_entry_added
    assert event_data['text'] == expected_text


@pytest.mark.asyncio
@pytest.mark.parametrize("log_method, expected_level", [
    ("assert", "error"),
    ("debug", "debug"),
    ("error", "error"),
    ("info", "info"),
    ("log", "info"),
    ("table", "info"),
    ("trace", "debug"),
    ("warn", "warning"),
])
async def test_console_log_level(bidi_session,
                                 current_session,
                                 wait_for_event,
                                 log_method,
                                 expected_level):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    # TODO: To be replaced with the BiDi implementation of execute_script.
    if log_method == 'assert':
        # assert has to be called with a first falsy argument to trigger a log.
        current_session.execute_script("console.assert(false, 'text')")
    else:
        current_session.execute_script(f"console.{log_method}('text')")

    event_data = await on_entry_added

    assert event_data['text'] == 'text'
    assert event_data['level'] == expected_level
    assert event_data['type'] == 'console'
    assert event_data['method'] == log_method
    assert isinstance(event_data['timestamp'], int)
