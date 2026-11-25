# Testing: https://w3c.github.io/aria/html-aam/#el-caption

TEST_HTML = "<table id='table'><caption id='target'>caption</caption><tr><td>cell</td></tr></table>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping
    # Relations: ATK_RELATION_LABEL_FOR with parent table

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.CAPTION
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_LABEL_FOR" in relations
    assert "table" in relations["RELATION_LABEL_FOR"]

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Relations: ATK_RELATION_LABEL_FOR with parent table

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Relations: IA2_RELATION_LABEL_FOR with parent table

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Other properties: The LabeledBy property for the parent table element points to the UIA element for the caption element.
