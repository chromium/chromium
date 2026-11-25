from tests.classic.perform_actions.support.refine import get_events


def test_listener_added_right_before_action(session, inline, wheel_chain):
    session.url = inline("""
        <style>body { margin: 0; }</style>
        <div id="target" style="width: 100px; height: 100px; background: blue"></div>
        <script>var allEvents = { events: [] };</script>
        """)

    # Keep the main thread busy after adding the listener, so that the wheel
    # action starts before any rendering update can reflect the new listener.
    # The action uses a viewport origin, as resolving an element origin would
    # have to wait for the main thread.
    session.execute_script("""
        document.getElementById("target").addEventListener("wheel", event => {
            allEvents.events.push({
                type: event.type,
                deltaX: event.deltaX,
                deltaY: event.deltaY,
                target: event.target.id,
            });
        });
        setTimeout(() => {
            const end = performance.now() + 500;
            while (performance.now() < end) { }
        }, 0);
        """)

    wheel_chain.scroll(50, 50, 5, 10, origin="viewport").perform()

    events = get_events(session)
    assert len(events) == 1
    assert events[0]["type"] == "wheel"
    assert events[0]["deltaX"] == 5
    assert events[0]["deltaY"] == 10
    assert events[0]["target"] == "target"
