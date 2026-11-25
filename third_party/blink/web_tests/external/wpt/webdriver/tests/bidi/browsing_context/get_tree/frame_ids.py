import pytest

from webdriver.bidi.modules.script import ContextTarget


pytestmark = pytest.mark.asyncio

CONTEXT_CREATED_EVENT = "browsingContext.contextCreated"
NAVIGATION_FAILED_EVENT = "browsingContext.navigationFailed"


async def get_child_ids(bidi_session, context_id):
    contexts = await bidi_session.browsing_context.get_tree(root=context_id)
    assert len(contexts) == 1
    return [child["context"] for child in contexts[0]["children"]]


async def test_ids_match_context_created_events(
    bidi_session, subscribe_events, wait_for_bidi_events, top_context, test_page_multiple_frames
):
    # The iframes are created while the document loads, so their ids must
    # survive the end of that load.
    await subscribe_events([CONTEXT_CREATED_EVENT])

    events = []

    async def on_event(method, data):
        events.append(data)

    remove_listener = bidi_session.add_event_listener(CONTEXT_CREATED_EVENT, on_event)

    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_multiple_frames, wait="complete"
    )
    await wait_for_bidi_events(events, 2)
    remove_listener()

    child_ids = await get_child_ids(bidi_session, top_context["context"])
    assert child_ids == [event["context"] for event in events]


@pytest.mark.parametrize(
    "expression",
    [
        "location.hash = 'foo'",
        "history.pushState(null, '', '#bar')",
        "history.replaceState(null, '', '#baz')",
    ],
    ids=["fragment", "push_state", "replace_state"],
)
async def test_same_document_navigation_keeps_ids(
    bidi_session, top_context, test_page_multiple_frames, expression
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_multiple_frames, wait="complete"
    )
    child_ids = await get_child_ids(bidi_session, top_context["context"])
    assert len(child_ids) == 2

    await bidi_session.script.evaluate(
        expression=expression,
        target=ContextTarget(top_context["context"]),
        await_promise=False,
    )

    assert await get_child_ids(bidi_session, top_context["context"]) == child_ids

    # The ids still address the frames.
    for child_id in child_ids:
        result = await bidi_session.script.evaluate(
            expression="document.body.textContent",
            target=ContextTarget(child_id),
            await_promise=False,
        )
        assert result["type"] == "string"


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_failed_navigation_keeps_ids(
    bidi_session, subscribe_events, wait_for_event, wait_for_future_safe, url,
    top_context, test_page_multiple_frames, domain
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_multiple_frames, wait="complete"
    )
    child_ids = await get_child_ids(bidi_session, top_context["context"])
    assert len(child_ids) == 2

    # A 204 response never commits, so the current document and its frames stay.
    await subscribe_events([NAVIGATION_FAILED_EVENT])
    on_navigation_failed = wait_for_event(NAVIGATION_FAILED_EVENT)
    no_content_url = url("/webdriver/tests/support/http_handlers/status.py?status=204", domain=domain)
    await bidi_session.script.evaluate(
        expression=f"location.href = '{no_content_url}'",
        target=ContextTarget(top_context["context"]),
        await_promise=False,
    )
    await wait_for_future_safe(on_navigation_failed)

    assert await get_child_ids(bidi_session, top_context["context"]) == child_ids

    for child_id in child_ids:
        result = await bidi_session.script.evaluate(
            expression="document.body.textContent",
            target=ContextTarget(child_id),
            await_promise=False,
        )
        assert result["type"] == "string"


async def test_navigating_away_removes_child_frames(
    bidi_session, top_context, test_page, test_page_multiple_frames
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_multiple_frames, wait="complete"
    )
    assert len(await get_child_ids(bidi_session, top_context["context"])) == 2

    # The old document may be kept in the back/forward cache with its child
    # navigables, but they are no longer part of the tree.
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page, wait="complete"
    )
    assert await get_child_ids(bidi_session, top_context["context"]) == []
