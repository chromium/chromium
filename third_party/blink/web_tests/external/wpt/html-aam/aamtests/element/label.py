import pytest

# Testing: https://w3c.github.io/aria/html-aam/#el-label

TEST_HTML = {
    "child-element": "<label id='target'>label <input id='input'></label>",
    "for-attribute": "<label id='target' for='input'>label</label><input id='input'>",
}

@pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
def test_atspi(atspi, session, inline, test_html):
    session.url = inline(test_html)

    # Spec:
    # Role: ATK_ROLE_LABEL
    # Relations: ATK_RELATION_LABEL_FOR for a child labelable element or labelable element referred by for attribute. Note, related labelable element provides ATK_RELATION_LABELLED_BY pointing to the label.

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.LABEL
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_LABEL_FOR" in relations
    assert "input" in relations["RELATION_LABEL_FOR"]
    reverse_node = atspi.find_node("input", session.url)
    reverse_relations = atspi.get_relations_dictionary_helper(reverse_node)
    assert "RELATION_LABELLED_BY" in reverse_relations
    assert "target" in reverse_relations["RELATION_LABELLED_BY"]

# @pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
# def test_axapi(axapi, session, inline, test_html):
#     session.url = inline(test_html)
#
#     # Spec:
#     # AXRole: AXGroup
#     # AXSubrole: (nil)
#     # AXRoleDescription: "group"

# @pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
# def test_ia2(ia2, session, inline, test_html):
#     session.url = inline(test_html)
#
#     # Spec:
#     # Roles: ROLE_SYSTEM_STATICTEXT; IA2_ROLE_LABEL
#     # Relations: IA2_RELATION_LABEL_FOR with a labelable element that is child to the label or referred to by the label element's for attribute. The associated labelable element has IA2_RELATION_LABELLED_BY pointing to the label.

# @pytest.mark.parametrize("test_html", TEST_HTML.values(), ids=TEST_HTML.keys())
# def test_uia(uia, session, inline, test_html):
#     session.url = inline(test_html)
#
#     # Spec:
#     # Control Type: Group
#     # Relations:
#     # When the label element contains a labelable element, the LabeledBy property for the element points to the UIA element for the label element.
#     # When the label element has a for attribute referencing a labelable element, the LabeledBy property for the referenced element points to the UIA element for the label element.
