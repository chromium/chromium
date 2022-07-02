import pytest

from webdriver.error import NoSuchAlertException

from tests.support.asserts import assert_error, assert_success


def get_computed_label(session, element_id):
    return session.transport.send(
        "GET", "session/{session_id}/element/{element_id}/computedlabel".format(
            session_id=session.session_id,
            element_id=element_id))


def test_no_browsing_context(session, closed_frame):
    response = get_computed_label(session, "foo")
    assert_error(response, "no such window")


@pytest.mark.parametrize("as_frame", [False, True], ids=["top_context", "child_context"])
def test_stale_element_reference(session, stale_element, as_frame):
    element = stale_element("<input>", "input", as_frame=as_frame)

    response = get_computed_label(session, element.id)
    assert_error(response, "stale element reference")


def test_no_user_prompt(session):
    response = get_computed_label(session, "foo")
    assert_error(response, "no such alert")


@pytest.mark.parametrize("html,tag,label", [
    ("<button>ok</button>", "button", "ok"),
    ("<button aria-labelledby=\"one two\"></button><div id=one>ok</div><div id=two>go</div>", "button", "ok go"),
    ("<button aria-label=foo>bar</button>", "button", "foo"),
    ("<label><input> foo</label>", "input", "foo"),
    ("<label for=b>foo<label><input id=b>", "input", "foo")])
def test_get_computed_label(session, inline, html, tag, label):
    session.url = inline("{0}".format(tag))
    element = session.find.css(tag, all=False)
    result = get_computed_label(session, element.id)
    assert_success(result, label)
