# Testing: https://w3c.github.io/aria/html-aam/#el-fieldset

TEST_HTML = "<fieldset id='target'><legend id='legend'>legend</legend>content</fieldset>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: Use WAI-ARIA mapping
    # Relations: ATK_RELATION_LABELLED_BY with first instance of a rendered child legend element

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.PANEL
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_LABELLED_BY" in relations
    assert "legend" in relations["RELATION_LABELLED_BY"]

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: Use WAI-ARIA mapping
#     # AXSubrole: AXFieldset
#     # AXDescription: value from the first instance of a rendered child legend element

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: Use WAI-ARIA mapping
#     # Relations: IA2_RELATION_LABELLED_BY with the first instance of a rendered child legend element

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: Use WAI-ARIA mapping
