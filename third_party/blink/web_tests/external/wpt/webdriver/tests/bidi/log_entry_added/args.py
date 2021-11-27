import pytest


@pytest.mark.asyncio
@pytest.mark.parametrize("data,remote_value", [
    ("undefined", {"type": "undefined"}),
    ("null", {"type": "null"}),
    ("bar", {"type": "string", "value": "bar"}),
    ("42", {"type": "number", "value": 42}),
    ("Number.NaN", {"type": "number", "value": "NaN"}),
    ("-0", {"type": "number", "value": "-0"}),
    ("Number.POSITIVE_INFINITY", {"type": "number", "value": "+Infinity"}),
    ("Number.NEGATIVE_INFINITY", {"type": "number", "value": "-Infinity"}),
    ("false", {"type": "boolean", "value": False}),
    ("42n", {"type": "bigint", "value": "42"}),
], ids=[
    "undefined",
    "null",
    "string",
    "number",
    "NaN",
    "-0",
    "+Infinity",
    "-Infinity",
    "boolean",
    "bigint",
])
async def test_primitive_types(bidi_session,
                               current_session,
                               wait_for_event,
                               data,
                               remote_value):
    await bidi_session.session.subscribe(events=["log.entryAdded"])

    on_entry_added = wait_for_event("log.entryAdded")

    if remote_value["type"] == "string":
        command = "console.log('foo', {!r})"
    else:
        command = "console.log('foo', {})"

    # TODO: To be replaced with the BiDi implementation of execute_script.
    current_session.execute_script(command.format(data))

    event_data = await on_entry_added

    args = event_data["args"]
    assert len(args) == 2

    # First arg is always the first argument as provided to console.log()
    assert args[0]["type"] == "string"
    assert args[0]["value"] == "foo"

    assert args[1]["type"] == remote_value["type"]
    if "value" in remote_value:
        assert args[1]["value"] == remote_value["value"]
    else:
        assert "value" not in args[1]
