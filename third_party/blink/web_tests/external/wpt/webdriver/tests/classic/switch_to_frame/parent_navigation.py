import pytest

from tests.support.classic.asserts import assert_success


def find_element(session, using, value):
    return session.transport.send(
        "POST", "session/{session_id}/element".format(**vars(session)),
        {"using": using, "value": value})


@pytest.mark.parametrize(
    "script",
    [
        "parent.location.hash = 'foo'",
        "parent.history.pushState(null, '', '#bar')",
        "parent.history.replaceState(null, '', '#baz')",
    ],
    ids=["fragment", "push_state", "replace_state"],
)
def test_same_document_navigation_of_parent_keeps_current_frame(session, inline, iframe, script):
    session.url = inline(iframe("<p>foo"))
    frame = session.find.css("iframe", all=False)
    session.switch_to_frame(frame)

    session.execute_script(script)

    response = find_element(session, "css selector", "p")
    assert_success(response)
