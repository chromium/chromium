# Testing: https://w3c.github.io/aria/html-aam/#el-table

TEST_HTML = "<table id='target'><caption id='caption'>caption</caption><tr><td>cell</td></tr></table>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping
    # Relations: ATK_RELATION_LABELLED_BY with first instance of a rendered child caption element

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.TABLE
    assert "xml-roles:table" in atspi.Accessible.get_attributes_as_array(node)
    assert atspi.Accessible.get_table_iface(node) is not None
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_LABELLED_BY" in relations
    assert "caption" in relations["RELATION_LABELLED_BY"]

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # AXDescription: value from the first instance of a rendered child caption element

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Relations: IA2_RELATION_LABELLED_BY with first instance of a rendered child caption element

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
