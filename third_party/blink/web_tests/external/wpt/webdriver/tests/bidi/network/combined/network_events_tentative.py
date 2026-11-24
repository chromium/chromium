import pytest

from tests.support.sync import AsyncPoll
from webdriver.bidi.modules.script import ContextTarget

from .. import (
    assert_before_request_sent_event,
    assert_response_event,
    PAGE_EMPTY_TEXT,
    BEFORE_REQUEST_SENT_EVENT,
    RESPONSE_COMPLETED_EVENT,
    RESPONSE_STARTED_EVENT,
)

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("user_context", ["default", "new"])
async def test_dedicated_worker_user_context(
    bidi_session,
    create_user_context,
    inline,
    url,
    setup_network_test,
    user_context,
):
    """
    Associating worker network events with `userContext` is tentative:
    https://github.com/w3c/webdriver-bidi/issues/1218
    """
    target_user_context = (
        await create_user_context()
        if user_context == "new"
        else user_context
    )
    target_context = await bidi_session.browsing_context.create(
        type_hint="tab", user_context=target_user_context
    )

    try:
        network_events = await setup_network_test(
            events=[
                BEFORE_REQUEST_SENT_EVENT,
                RESPONSE_STARTED_EVENT,
                RESPONSE_COMPLETED_EVENT,
            ],
            test_url=inline(""),
            context=target_context["context"],
        )

        text_url = url(PAGE_EMPTY_TEXT)
        worker_url = inline(
            f"fetch('{text_url}').then(r => r.text())"
            ".then(() => self.postMessage('done'));",
            doctype="js",
        )
        await bidi_session.script.call_function(
            function_declaration="""(workerUrl) => new Promise(resolve => {
                const worker = new Worker(workerUrl);
                worker.onmessage = () => {
                    worker.terminate();
                    resolve();
                };
            })""",
            arguments=[{"type": "string", "value": worker_url}],
            target=ContextTarget(target_context["context"]),
            await_promise=True,
        )

        wait = AsyncPoll(bidi_session, timeout=2)
        await wait.until(
            lambda _: any(
                e["request"]["url"] == text_url
                for e in network_events[RESPONSE_COMPLETED_EVENT]
            )
        )

        fetch_before_events = [
            e
            for e in network_events[BEFORE_REQUEST_SENT_EVENT]
            if e["request"]["url"] == text_url
        ]
        fetch_started_events = [
            e
            for e in network_events[RESPONSE_STARTED_EVENT]
            if e["request"]["url"] == text_url
        ]
        fetch_completed_events = [
            e
            for e in network_events[RESPONSE_COMPLETED_EVENT]
            if e["request"]["url"] == text_url
        ]

        assert len(fetch_before_events) == 1
        assert len(fetch_started_events) == 1
        assert len(fetch_completed_events) == 1

        expected_request = {"method": "GET", "url": text_url}
        expected_response = {"url": text_url}
        for event in (
            fetch_before_events[0],
            fetch_started_events[0],
            fetch_completed_events[0],
        ):
            assert "userContext" in event

        assert_before_request_sent_event(
            fetch_before_events[0],
            expected_event={
                "request": expected_request,
                "context": None,
                "userContext": target_user_context,
            },
        )
        assert_response_event(
            fetch_started_events[0],
            expected_event={
                "response": expected_response,
                "context": None,
                "userContext": target_user_context,
            },
        )
        assert_response_event(
            fetch_completed_events[0],
            expected_event={
                "response": expected_response,
                "context": None,
                "userContext": target_user_context,
            },
        )
    finally:
        if user_context == "default":
            await bidi_session.browsing_context.close(
                context=target_context["context"]
            )
