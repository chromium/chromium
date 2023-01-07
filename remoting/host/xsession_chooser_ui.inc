// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The user interface definition used for the session chooser.

namespace remoting {

namespace {

const char UI[] = R"UI_Definition(
  <interface>
    <requires lib="gtk+" version="3.2"/>
    <object class="GtkListStore" id="session_store">
      <columns>
        <column type="guint"/>
        <column type="gchararray"/>
        <column type="gchararray"/>
      </columns>
    </object>
    <object class="GtkDialog" id="dialog">
      <property name="can_focus">False</property>
      <property name="default_width">600</property>
      <property name="default_height">400</property>
      <property name="type_hint">dialog</property>
      <child>
        <placeholder/>
      </child>
      <child internal-child="vbox">
        <object class="GtkBox" id="layout_box">
          <property name="can_focus">False</property>
          <property name="orientation">vertical</property>
          <property name="spacing">2</property>
          <child internal-child="action_area">
            <object class="GtkButtonBox" id="button_box">
              <property name="can_focus">False</property>
              <property name="layout_style">end</property>
              <child>
                <object class="GtkButton" id="ok_button">
                  <property name="label">gtk-ok</property>
                  <property name="visible">True</property>
                  <property name="can_focus">True</property>
                  <property name="receives_default">True</property>
                  <property name="use_stock">True</property>
                </object>
                <packing>
                  <property name="expand">True</property>
                  <property name="fill">True</property>
                  <property name="position">0</property>
                </packing>
              </child>
            </object>
            <packing>
              <property name="expand">False</property>
              <property name="fill">False</property>
              <property name="position">0</property>
            </packing>
          </child>
          <child>
            <object class="GtkLabel" id="message">
              <property name="visible">True</property>
              <property name="can_focus">False</property>
              <property name="wrap">True</property>
            </object>
            <packing>
              <property name="expand">False</property>
              <property name="fill">True</property>
              <property name="position">1</property>
            </packing>
          </child>
          <child>
            <object class="GtkScrolledWindow" id="scroll_container">
              <property name="visible">True</property>
              <property name="can_focus">True</property>
              <property name="shadow_type">in</property>
              <child>
                <object class="GtkTreeView" id="session_list">
                  <property name="visible">True</property>
                  <property name="can_focus">True</property>
                  <property name="model">session_store</property>
                  <child internal-child="selection">
                    <object class="GtkTreeSelection" id="session_selection">
                      <property name="mode">browse</property>
                    </object>
                  </child>
                  <child>
                    <object class="GtkTreeViewColumn" id="name_column">
                      <child>
                        <object class="GtkCellRendererText" id="name_text"/>
                        <attributes>
                          <attribute name="text">1</attribute>
                        </attributes>
                      </child>
                    </object>
                  </child>
                  <child>
                    <object class="GtkTreeViewColumn" id="comment_column">
                      <child>
                        <object class="GtkCellRendererText" id="comment_text"/>
                        <attributes>
                          <attribute name="text">2</attribute>
                        </attributes>
                      </child>
                    </object>
                  </child>
                </object>
              </child>
            </object>
            <packing>
              <property name="expand">True</property>
              <property name="fill">True</property>
              <property name="position">2</property>
            </packing>
          </child>
        </object>
      </child>
    </object>
  </interface>
)UI_Definition";

}  // namespace

}  // namespace remoting
