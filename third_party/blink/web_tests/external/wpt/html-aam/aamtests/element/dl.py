# Testing: https://w3c.github.io/aria/html-aam/#el-dl

TEST_HTML = "<dl id='target'><dt>term</dt><dd>definition</dd></dl>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_DESCRIPTION_LIST

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.DESCRIPTION_LIST

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXList
#     # AXSubrole: AXDefinitionList
#     # AXRoleDescription: "definition list"

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: ROLE_SYSTEM_LIST
#     # States: STATE_SYSTEM_READONLY

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: List
