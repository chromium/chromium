# Testing: https://w3c.github.io/aria/html-aam/#el-legend

TEST_HTML = "<fieldset id='fieldset'><legend id='target'>legend</legend>content</fieldset>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_LABEL
    # Relations: ATK_RELATION_LABEL_FOR with parent fieldset element

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.LABEL
    relations = atspi.get_relations_dictionary_helper(node)
    assert "RELATION_LABEL_FOR" in relations
    assert "fieldset" in relations["RELATION_LABEL_FOR"]

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXGroup
#     # AXSubrole: (nil)
#     # AXRoleDescription: "group"

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Roles: ROLE_SYSTEM_STATICTEXT; IA2_ROLE_LABEL
#     # Relations: IA2_RELATION_LABEL_FOR with the parent fieldset

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: Text
#     # Other properties: The LabeledBy property for the parent fieldset points to the UIA element for the legend element.
