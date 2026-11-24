import asyncio

import pytest

from tests.bidi import recursive_compare
from webdriver.bidi.modules.script import ContextTarget
from . import OFFLINE_NETWORK_CONDITIONS

pytestmark = pytest.mark.asyncio


async def test_navigator_online(bidi_session, top_context,
        get_navigator_online):
    assert await get_navigator_online(top_context)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=OFFLINE_NETWORK_CONDITIONS,
        contexts=[top_context["context"]])
    assert not await get_navigator_online(top_context)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=None,
        contexts=[top_context["context"]])
    assert await get_navigator_online(top_context)


async def test_fetch(bidi_session, top_context, url, get_can_fetch):
    # Navigate away from about:blank to allow fetch requests.
    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=url(f"/common/blank.html"),
        wait="complete")

    assert await get_can_fetch(top_context)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=OFFLINE_NETWORK_CONDITIONS,
        contexts=[top_context["context"]])
    assert not await get_can_fetch(top_context)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=None,
        contexts=[top_context["context"]])
    assert await get_can_fetch(top_context)


async def test_fetch_keepalive(bidi_session, top_context, url, get_can_fetch):
    # Navigate away from about:blank to allow fetch requests.
    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=url("/common/blank.html"),
        wait="complete")

    fetch_url = url("/common/dummy.json")
    fetch_options = {"cache": "no-store", "keepalive": True}

    assert await get_can_fetch(top_context, fetch_url, fetch_options)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=OFFLINE_NETWORK_CONDITIONS,
        contexts=[top_context["context"]])
    assert not await get_can_fetch(top_context, fetch_url, fetch_options)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=None,
        contexts=[top_context["context"]])
    assert await get_can_fetch(top_context, fetch_url, fetch_options)


async def test_navigate(bidi_session, top_context, url,
        get_can_navigate):
    assert await get_can_navigate(top_context)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=OFFLINE_NETWORK_CONDITIONS,
        contexts=[top_context["context"]])
    assert not await get_can_navigate(top_context)
    await bidi_session.emulation.set_network_conditions(
        network_conditions=None,
        contexts=[top_context["context"]])
    assert await get_can_navigate(top_context)


async def test_webtransport_disconnected(bidi_session, configuration,
        top_context, url, server_config):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=url("/common/blank.html"),
        wait="complete")

    webtransport_url = (
        f"https://{server_config['browser_host']}:"
        f"{server_config['ports']['webtransport-h3'][0]}"
        "/webtransport/handlers/echo.py"
    )
    await bidi_session.script.call_function(
        function_declaration="""async (url) => {
            window.webTransport = new WebTransport(url);
            window.webTransport.closed.catch(() => {});
            await window.webTransport.ready;
        }""",
        arguments=[{"type": "string", "value": webtransport_url}],
        target=ContextTarget(top_context["context"]),
        await_promise=True,
    )

    timeout = 2.0 * configuration["timeout_multiplier"]
    established_task = None
    new_task = None
    try:
        await bidi_session.emulation.set_network_conditions(
            network_conditions=OFFLINE_NETWORK_CONDITIONS,
            contexts=[top_context["context"]])

        # Shield the evaluation tasks so a timeout does not cancel the
        # underlying BiDi command futures before `finally` closes the sessions.
        established_task = asyncio.create_task(
            bidi_session.script.evaluate(
                expression="""window.webTransport.closed.then(
                    () => "closed", () => "failed")""",
                target=ContextTarget(top_context["context"]),
                await_promise=True,
            )
        )
        established_result = await asyncio.wait_for(
            asyncio.shield(established_task),
            timeout=timeout,
        )

        new_task = asyncio.create_task(
            bidi_session.script.call_function(
                function_declaration="""async (url) => {
                    window.newWebTransport = new WebTransport(`${url}?new`);
                    window.newWebTransport.closed.catch(() => {});
                    return window.newWebTransport.ready.then(
                        () => "ready", () => "failed");
                }""",
                arguments=[{"type": "string", "value": webtransport_url}],
                target=ContextTarget(top_context["context"]),
                await_promise=True,
            )
        )
        new_result = await asyncio.wait_for(
            asyncio.shield(new_task),
            timeout=timeout,
        )

        assert established_result["value"] == "failed"
        assert new_result["value"] == "failed"
    finally:
        await bidi_session.emulation.set_network_conditions(
            network_conditions=None,
            contexts=[top_context["context"]])
        await bidi_session.script.evaluate(
            expression="""(() => {
                window.webTransport?.close();
                window.newWebTransport?.close();
            })()""",
            target=ContextTarget(top_context["context"]),
            await_promise=False,
        )
        tasks = [
            task
            for task in (established_task, new_task)
            if task is not None
        ]
        pending_tasks = [task for task in tasks if not task.done()]
        if pending_tasks:
            await asyncio.wait(pending_tasks, timeout=timeout)
        for task in tasks:
            if task.done() and not task.cancelled():
                task.exception()


async def test_window_offline_online_events(bidi_session, top_context, url,
        subscribe_events, wait_for_event, wait_for_future_safe):
    await subscribe_events(["script.message"])
    await bidi_session.script.call_function(
        function_declaration="""(channel)=>{
            window.addEventListener("offline", (e) => {{
                channel("offline, isTrusted: "+ e.isTrusted);
            }});
            window.addEventListener("online", (e) => {{
                channel("online, isTrusted: "+ e.isTrusted);
            }});
        }""",
        arguments=[{
            "type": "channel",
            "value": {
                "channel": "channel_name"
            }
        }],
        target=ContextTarget(top_context["context"]),
        await_promise=True,
    )

    on_script_message = wait_for_event("script.message")
    await bidi_session.emulation.set_network_conditions(
        network_conditions=OFFLINE_NETWORK_CONDITIONS,
        contexts=[top_context["context"]])

    # Wait for the window `offline` event.
    event_data = await wait_for_future_safe(on_script_message)
    recursive_compare(
        {
            'channel': 'channel_name',
            'data': {
                'type': 'string',
                'value': 'offline, isTrusted: true'
            },
        }, event_data,
    )

    on_script_message = wait_for_event("script.message")
    await bidi_session.emulation.set_network_conditions(
        network_conditions=None,
        contexts=[top_context["context"]])

    # Wait for the window `online` event.
    event_data = await wait_for_future_safe(on_script_message)
    recursive_compare(
        {
            'channel': 'channel_name',
            'data': {
                'type': 'string',
                'value': 'online, isTrusted: true'
            },
        }, event_data,
    )


# TODO: implement test_websocket_disconnected.
# TODO: implement test_service_worker_fetch.
# TODO: implement test_service_worker_websocket_disconnected.

