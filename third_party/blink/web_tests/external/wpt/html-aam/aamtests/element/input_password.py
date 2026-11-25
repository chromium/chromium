# Testing: https://w3c.github.io/aria/html-aam/#el-input-password

TEST_HTML = "<input id='target' type='password'>"
TEST_HTML_READONLY = "<input id='target' type='password' readonly>"

def test_atspi_editable(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_PASSWORD_TEXT
    # States: ATK_STATE_SINGLE_LINE; ATK_STATE_READ_ONLY if readonly, otherwise ATK_STATE_EDITABLE

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.PASSWORD_TEXT
    assert "STATE_SINGLE_LINE" in atspi.get_state_list_helper(node)
    assert "STATE_EDITABLE" in atspi.get_state_list_helper(node)

def test_atspi_readonly(atspi, session, inline):
    session.url = inline(TEST_HTML_READONLY)

    # Spec:
    # Role: ATK_ROLE_PASSWORD_TEXT
    # States: ATK_STATE_SINGLE_LINE; ATK_STATE_READ_ONLY if readonly, otherwise ATK_STATE_EDITABLE

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.PASSWORD_TEXT
    assert "STATE_SINGLE_LINE" in atspi.get_state_list_helper(node)
    assert "STATE_READ_ONLY" in atspi.get_state_list_helper(node)
    assert "STATE_EDITABLE" not in atspi.get_state_list_helper(node)

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXTextField
#     # AXSubrole: AXSecureTextField
#     # AXRoleDescription: "secure text field"

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: ROLE_SYSTEM_TEXT
#     # States: STATE_SYSTEM_PROTECTED; IA2_STATE_SINGLE_LINE; STATE_SYSTEM_READONLY if readonly, otherwise IA2_STATE_EDITABLE

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: Edit
#     # Other properties: isPassword=true
