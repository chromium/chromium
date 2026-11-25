# Testing: https://w3c.github.io/aria/html-aam/#el-details

TEST_HTML = "<details id='target'><summary>summary</summary>content</details>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping
    # Relations: "ATK_RELATION_DETAILS_FOR"

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.PANEL
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_DETAILS_FOR" in relations

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

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
#     # Localized Control Type: "details"
