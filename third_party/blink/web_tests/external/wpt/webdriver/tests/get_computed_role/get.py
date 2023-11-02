import pytest

from webdriver.error import NoSuchAlertException

from tests.support.asserts import assert_error, assert_success


def get_computed_role(session, element_id):
    return session.transport.send(
        "GET", "session/{session_id}/element/{element_id}/computedrole".format(
            session_id=session.session_id,
            element_id=element_id))


def test_no_browsing_context(session, closed_frame):
    response = get_computed_role(session, "foo")
    assert_error(response, "no such window")


@pytest.mark.parametrize("as_frame", [False, True], ids=["top_context", "child_context"])
def test_stale_element_reference(session, stale_element, as_frame):
    element = stale_element("<input>", "input", as_frame=as_frame)

    response = get_computed_role(session, element.id)
    assert_error(response, "stale element reference")


@pytest.mark.parametrize("html,tag,expected", [
    ("<li role=menuitem>foo", "li", "menuitem"),
    ("<input role=searchbox>", "input", "searchbox"),
    ("<img role=presentation>", "img", "presentation")])
def test_computed_roles(session, inline, html, tag, expected):
    session.url = inline(html)
    element = session.find.css(tag, all=False)
    result = get_computed_role(session, element.id)
    assert_success(result, expected)
