import pytest

# Testing: https://w3c.github.io/aria/html-aam/#el-input-textetc-autocomplete

TEST_HTML = {
    "text": "<input id='target' type='text' list='list'><datalist id='list'><option value='option'></option></datalist>",
    "search": "<input id='target' type='search' list='list'><datalist id='list'><option value='option'></option></datalist>",
    "tel": "<input id='target' type='tel' list='list'><datalist id='list'><option value='option'></option></datalist>",
    "url": "<input id='target' type='url' list='list'><datalist id='list'><option value='option'></option></datalist>",
    "email": "<input id='target' type='email' list='list'><datalist id='list'><option value='option'></option></datalist>",
}

@pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
def test_atspi(atspi, session, inline, test_html):
    session.url = inline(test_html)

    # Spec:
    # Use WAI-ARIA mapping

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.COMBO_BOX
    assert "STATE_EXPANDABLE" in atspi.get_state_list_helper(node)
    assert "STATE_HAS_POPUP" in atspi.get_state_list_helper(node)

# @pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
# def test_axapi(axapi, session, inline, test_html):
#     session.url = inline(test_html)
#
#     # Spec:
#     # Use WAI-ARIA mapping

# @pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
# def test_ia2(ia2, session, inline, test_html):
#     session.url = inline(test_html)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Object attributes: text-input-type:as per input type

# @pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
# def test_uia(uia, session, inline, test_html):
#     session.url = inline(test_html)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Other properties: ControllerFor points to the suggestions source element
