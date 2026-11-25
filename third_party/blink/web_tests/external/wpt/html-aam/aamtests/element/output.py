# Testing: https://w3c.github.io/aria/html-aam/#el-output

TEST_HTML = "<label id='label' for='target'>label</label><output id='target'>content</output>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping
    # Relations: ATK_RELATION_LABELLED_BY with associated label element

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.STATUS_BAR
    assert "container-live:polite" in atspi.Accessible.get_attributes_as_array(node)
    assert "live:polite" in atspi.Accessible.get_attributes_as_array(node)
    assert "container-live-role:status" in atspi.Accessible.get_attributes_as_array(node)
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_LABELLED_BY" in relations
    assert "label" in relations["RELATION_LABELLED_BY"]

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Relations: IA2_RELATION_LABELLED_BY with associated label element

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
