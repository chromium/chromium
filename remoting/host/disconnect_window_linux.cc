// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include <memory>
#include <numbers>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

class DisconnectWindowGtk : public HostWindow {
 public:
  DisconnectWindowGtk();

  DisconnectWindowGtk(const DisconnectWindowGtk&) = delete;
  DisconnectWindowGtk& operator=(const DisconnectWindowGtk&) = delete;

  ~DisconnectWindowGtk() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  gboolean OnDelete(GtkWidget* window, GdkEvent* event);
  void OnClicked(GtkButton* button);
  gboolean OnConfigure(GtkWidget* widget, GdkEventConfigure* event);
  gboolean OnDraw(GtkWidget* widget, cairo_t* cr);
  gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  raw_ptr<GtkWidget> disconnect_window_;
  raw_ptr<GtkWidget> message_;
  raw_ptr<GtkWidget> button_;

  // Used to distinguish resize events from other types of "configure-event"
  // notifications.
  int current_width_;
  int current_height_;

  std::vector<ScopedGSignal> signals_;
};

// Helper function for creating a rectangular path with rounded corners, as
// Cairo doesn't have this facility.  |radius| is the arc-radius of each
// corner.  The bounding rectangle extends from (0, 0) to (width, height).
void AddRoundRectPath(cairo_t* cairo_context,
                      int width,
                      int height,
                      int radius) {
  cairo_new_sub_path(cairo_context);
  cairo_arc(cairo_context, width - radius, radius, radius,
            -std::numbers::pi / 2, 0);
  cairo_arc(cairo_context, width - radius, height - radius, radius, 0,
            std::numbers::pi / 2);
  cairo_arc(cairo_context, radius, height - radius, radius,
            std::numbers::pi / 2, std::numbers::pi);
  cairo_arc(cairo_context, radius, radius, radius, std::numbers::pi,
            3 * std::numbers::pi / 2);
  cairo_close_path(cairo_context);
}

// Renders the disconnect window background.
void DrawBackground(cairo_t* cairo_context, int width, int height) {
  // Set the arc radius for the corners.
  const int kCornerRadius = 6;

  // Initialize the whole bitmap to be transparent.
  cairo_save(cairo_context);
  cairo_set_source_rgba(cairo_context, 0, 0, 0, 0);
  cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_context);
  cairo_restore(cairo_context);

  AddRoundRectPath(cairo_context, width, height, kCornerRadius);
  cairo_clip(cairo_context);

  // Paint the whole bitmap one color.
  cairo_set_source_rgb(cairo_context, 0.91, 0.91, 0.91);
  cairo_paint(cairo_context);

  // Paint the round-rectangle edge.
  cairo_set_source_rgb(cairo_context, 0.13, 0.69, 0.11);
  cairo_set_line_width(cairo_context, 6);
  AddRoundRectPath(cairo_context, width, height, kCornerRadius);
  cairo_stroke(cairo_context);

  // Render the window-gripper.  In order for a straight line to light up
  // single pixels, Cairo requires the coordinates to have fractional
  // components of 0.5 (so the "/ 2" is a deliberate integer division).
  double gripper_top = height / 2 - 10.5;
  double gripper_bottom = height / 2 + 10.5;
  cairo_set_line_width(cairo_context, 1);

  double x = 12.5;
  cairo_set_source_rgb(cairo_context, 0.70, 0.70, 0.70);
  cairo_move_to(cairo_context, x, gripper_top);
  cairo_line_to(cairo_context, x, gripper_bottom);
  cairo_stroke(cairo_context);
  x += 3;
  cairo_move_to(cairo_context, x, gripper_top);
  cairo_line_to(cairo_context, x, gripper_bottom);
  cairo_stroke(cairo_context);

  x -= 2;
  cairo_set_source_rgb(cairo_context, 0.97, 0.97, 0.97);
  cairo_move_to(cairo_context, x, gripper_top);
  cairo_line_to(cairo_context, x, gripper_bottom);
  cairo_stroke(cairo_context);
  x += 3;
  cairo_move_to(cairo_context, x, gripper_top);
  cairo_line_to(cairo_context, x, gripper_bottom);
  cairo_stroke(cairo_context);
}

DisconnectWindowGtk::DisconnectWindowGtk()
    : disconnect_window_(nullptr), current_width_(0), current_height_(0) {}

DisconnectWindowGtk::~DisconnectWindowGtk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disconnect_window_) {
    gtk_widget_destroy(disconnect_window_);
    disconnect_window_ = nullptr;
  }
}

void DisconnectWindowGtk::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_session_control_.get());
  DCHECK(client_session_control.get());
  DCHECK(!disconnect_window_);

  client_session_control_ = client_session_control;

  // Create the window.
  disconnect_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWindow* window = GTK_WINDOW(disconnect_window_.get());

  auto connect = [&](auto* sender, const char* detailed_signal, auto receiver) {
    // Unretained() is safe since DisconnectWindowGtk will own the
    // ScopedGSignal.
    signals_.emplace_back(
        sender, detailed_signal,
        base::BindRepeating(receiver, base::Unretained(this)));
  };

  connect(disconnect_window_.get(), "delete-event",
          &DisconnectWindowGtk::OnDelete);
  gtk_window_set_title(window,
                       l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str());
  gtk_window_set_resizable(window, FALSE);

  // Try to keep the window always visible.
  gtk_window_stick(window);
  gtk_window_set_keep_above(window, TRUE);

  // Remove window titlebar.
  gtk_window_set_decorated(window, FALSE);

  // In case the titlebar is still there, try to remove some of the buttons.
  // Utility windows have no minimize button or taskbar presence.
  gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_deletable(window, FALSE);

  // Allow custom rendering of the background pixmap.
#if !GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_app_paintable(disconnect_window_, TRUE);
#endif
  connect(disconnect_window_.get(), "draw", &DisconnectWindowGtk::OnDraw);

  // Handle window resizing, to regenerate the background pixmap and window
  // shape bitmap.  The stored width & height need to be initialized here
  // in case the window is created a second time (the size of the previous
  // window would be remembered, preventing the generation of bitmaps for the
  // new window).
  current_height_ = current_width_ = 0;
  connect(disconnect_window_.get(), "configure-event",
          &DisconnectWindowGtk::OnConfigure);

  // Handle mouse events to allow the user to drag the window around.
#if !GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_events(disconnect_window_, GDK_BUTTON_PRESS_MASK);
#endif
  connect(disconnect_window_.get(), "button-press-event",
          &DisconnectWindowGtk::OnButtonPress);

  // All magic numbers taken from screen shots provided by UX.
  // The alignment sets narrow margins at the top and bottom, compared with
  // left and right.  The left margin is made larger to accommodate the
  // window movement gripper.
  GtkWidget* button_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_set_homogeneous(GTK_BOX(button_row), FALSE);

#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_widget_set_margin_start(GTK_WIDGET(button_row), 24);
  gtk_widget_set_margin_end(GTK_WIDGET(button_row), 12);
  gtk_widget_set_margin_top(GTK_WIDGET(button_row), 8);
  gtk_widget_set_margin_bottom(GTK_WIDGET(button_row), 8);
  gtk_container_add(GTK_CONTAINER(window), button_row);
#else
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  GtkWidget* align = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 8, 24, 12);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  gtk_container_add(GTK_CONTAINER(window), align);
  gtk_container_add(GTK_CONTAINER(align), button_row);
#endif

  button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_STOP_SHARING_BUTTON).c_str());
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_box_pack_end(GTK_BOX(button_row), button_);
#else
  gtk_box_pack_end(GTK_BOX(button_row), button_, FALSE, FALSE, 0);
#endif

  connect(GTK_BUTTON(button_.get()), "clicked",
          &DisconnectWindowGtk::OnClicked);

  message_ = gtk_label_new(nullptr);
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_box_pack_end(GTK_BOX(button_row), message_);
#else
  gtk_box_pack_end(GTK_BOX(button_row), message_, FALSE, FALSE, 0);
#endif

  // Override any theme setting for the text color, so that the text is
  // readable against the window's background pixmap.
  PangoAttrList* attributes = pango_attr_list_new();
  PangoAttribute* text_color = pango_attr_foreground_new(0, 0, 0);
  pango_attr_list_insert(attributes, text_color);
  gtk_label_set_attributes(GTK_LABEL(message_.get()), attributes);
  pango_attr_list_unref(attributes);

#if !GTK_CHECK_VERSION(3, 90, 0)
  // GTK4 always uses an RGBA visual for windows.
  GdkScreen* screen = gtk_widget_get_screen(disconnect_window_);
  GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
  if (visual) {
    gtk_widget_set_visual(disconnect_window_, visual);
  }

  // GTK4 shows windows by default.
  gtk_widget_show_all(disconnect_window_);
#endif

  // Extract the user name from the JID.
  std::string client_jid = client_session_control_->client_jid();
  std::u16string username =
      base::UTF8ToUTF16(client_jid.substr(0, client_jid.find('/')));
  gtk_label_set_text(
      GTK_LABEL(message_.get()),
      l10n_util::GetStringFUTF8(IDS_MESSAGE_SHARED, username).c_str());
  gtk_window_present(window);
}

void DisconnectWindowGtk::OnClicked(GtkButton* button) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client_session_control_.get()) {
    client_session_control_->DisconnectSession(ErrorCode::OK);
  }
}

gboolean DisconnectWindowGtk::OnDelete(GtkWidget* window, GdkEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client_session_control_.get()) {
    client_session_control_->DisconnectSession(ErrorCode::OK);
  }
  return TRUE;
}

gboolean DisconnectWindowGtk::OnConfigure(GtkWidget* widget,
                                          GdkEventConfigure* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only generate bitmaps if the size has actually changed.
  if (event->width == current_width_ && event->height == current_height_) {
    return FALSE;
  }

  current_width_ = event->width;
  current_height_ = event->height;

  // gdk_window_set_back_pixmap() is not supported in GDK3, and
  // background drawing is handled in OnDraw().
  return FALSE;
}

gboolean DisconnectWindowGtk::OnDraw(GtkWidget* widget, cairo_t* cr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DrawBackground(cr, current_width_, current_height_);
  return FALSE;
}

gboolean DisconnectWindowGtk::OnButtonPress(GtkWidget* widget,
                                            GdkEventButton* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gtk_window_begin_move_drag(GTK_WINDOW(disconnect_window_.get()),
                             event->button, event->x_root, event->y_root,
                             event->time);
  return FALSE;
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return std::make_unique<DisconnectWindowGtk>();
}

}  // namespace remoting
