# Testing: https://w3c.github.io/aria/html-aam/#el-summary

TEST_HTML = "<details><summary id='target'>summary</summary>content</details>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ROLE_TOGGLE_BUTTON
    # Relations: ATK_RELATION_DETAILS

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.TOGGLE_BUTTON
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_DETAILS" in relations

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXDisclosureTriangle
#     # AXSubrole: (nil)
#     # AXRoleDescription: "disclosure triangle"

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: ROLE_SYSTEM_PUSHBUTTON
#     # States: STATE_SYSTEM_EXPANDED / STATE_SYSTEM_COLLAPSED
#     # Actions: expand / collapse

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: Button
#     # Control Pattern: ExpandCollapse
