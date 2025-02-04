import pytest

from tests.support.sync import AsyncPoll
from ... import create_console_api_message, recursive_compare


@pytest.mark.asyncio
async def test_subscribe_one_user_context(bidi_session, subscribe_events, create_user_context):
    user_context = await create_user_context()

    default_context = await bidi_session.browsing_context.create(
        type_hint="tab",
        user_context="default"
    )

    other_context = await bidi_session.browsing_context.create(
        type_hint="tab",
        user_context=user_context
    )

    await subscribe_events(events=["log.entryAdded"], user_contexts=[user_context])

    # Track all received log.entryAdded events in the events array
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener("log.entryAdded", on_event)

    await create_console_api_message(bidi_session, default_context, "text1")
    await create_console_api_message(bidi_session, other_context, "text2")

    wait = AsyncPoll(
        bidi_session, message="Didn't receive expected log events"
    )
    await wait.until(lambda _: len(events) >= 1)

    assert len(events) == 1
    recursive_compare(
        {
            "text": "text2",
        },
        events[0],
    )

    remove_listener()


@pytest.mark.asyncio
async def test_subscribe_multiple_user_contexts(bidi_session, subscribe_events, create_user_context):
    user_context = await create_user_context()

    default_context = await bidi_session.browsing_context.create(
        type_hint="tab",
        user_context="default"
    )

    other_context = await bidi_session.browsing_context.create(
        type_hint="tab",
        user_context=user_context
    )

    await subscribe_events(events=["log.entryAdded"], user_contexts=[user_context, "default"])

    # Track all received log.entryAdded events in the events array
    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener("log.entryAdded", on_event)

    await create_console_api_message(bidi_session, default_context, "text1")
    await create_console_api_message(bidi_session, other_context, "text2")

    wait = AsyncPoll(
        bidi_session, message="Didn't receive expected log events"
    )
    await wait.until(lambda _: len(events) >= 2)

    assert len(events) == 2

    remove_listener()
